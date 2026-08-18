# 四驱控制器 CAN 加入 OTA（UDS）功能 —— 完整修改步骤

> 本文档根据以下两个目录的逐文件对比整理：
> - **原代码（未移植 OTA）**：`D:\260706_NL_0722备份\app\can` + `D:\260706_NL_0722备份\startup\main.c`
> - **已移植 OTA 代码（当前可用）**：`D:\260706_NL\project\app1\app\can`（含 `uds\`）+ `D:\260706_NL\project\app1\startup\main.c`
>
> 参考文档：`can_update.md`、`CAN模块拆分说明.md`、`四驱APPOTA优化.md`、`四驱新APP分区.md`、`uds移植.md`

---

## 一、结论（先看这里）

给原四驱 CAN 工程加入 OTA 功能，一共要做 **6 类改动**：

| # | 类别 | 改什么 | 关键文件 |
|---|------|--------|----------|
| 1 | 工程配置 | Flash 分区（链接地址 0x0 → 0x1A000）、IROM1 尺寸、新增源码组、Include 路径、生成 .bin、加大 Stack/Heap | `MDK\*.uvprojx`、`MDK\startup_hc32f460.s` |
| 2 | 宏开关 | 新增 `SYS_ENABLE_UDS`，使能 SWDT | `config\sys_config.h`、`startup\hc32f4xx_conf.h` |
| 3 | CAN HAL 增强 | TX 完成回调 + 发送忙查询 + 统一中断入口（ISOTP 多帧/流控依赖） | `app\can\can_module.h/c`、`app\can\can_hw.c`、`app\can\app_can.c` |
| 4 | CAN 三层拆分 | 从 `can_module.h` 抽出协议层 `can_protocol.h`，HAL 层与业务解耦（供 boot 复用） | `app\can\can_protocol.h`（新）、`app\can\can_module.h`、`app\can\app_can.h` |
| 5 | 新增 UDS 协议栈 | 整套 UDS/ISOTP/Flash 下载（**9 个 .c + 14 个 .h**，全部新增在 `app\can\uds\`） | `app\can\uds\*` |
| 6 | 业务串联 | `app_can_receive()` 的 `default` 分支转发 UDS；`app_can_init()` 初始化 UDS；`main()` 轮询 UDS；Phase 3 补发 ACK | `app\can\app_can.c`、`startup\main.c`、`drivers\...\system_hc32f460.c`、`drivers\drv\drv_mcu_flash.c` |

> ⚠️ **前提**：四驱工程**不编译 bootloader**。OTA 的 Phase 2（0x34/0x36/0x37 下载、跳转槽写入）由 **OTA 工程（`D:\ota_ddl3.3_v.3.1`）的 boot 固件**烧在 `0x0` 完成。四驱工程只做 Phase 1（0x31 → 写共享区 → 延迟复位）和 Phase 3（启动补发 `51 01`）。

---

## 二、总体架构与调用链

### 2.1 三层 CAN 架构（拆分后）

```
┌────────────────────────────────────────────────────────────┐
│ app_can.h / app_can.c      应用层：电机状态机、收发调度、业务发布 │
├────────────────────────────────────────────────────────────┤
│ can_protocol.h             协议层：CAN ID 位域结构体、枚举、联合体 │
├────────────────────────────────────────────────────────────┤
│ can_module.h/c + can_hw.c  HAL 层：GPIO/波特率/滤波器/中断/收发原语 │
└────────────────────────────────────────────────────────────┘
        ↑ 共享 can_handle_t can_handle
┌────────────────────────────────────────────────────────────┐
│ app/can/uds/*              UDS/ISOTP/Flash 下载协议栈          │
└────────────────────────────────────────────────────────────┘
```

### 2.2 接收数据流（唯一 RX 通路）

```
CAN 中断 → can_module_irq_handler() → can_rx 环形缓存
  → 主循环 app_can_receive()            (app/can/app_can.c)
      ├─ switch 匹配 0x18FF9DF0 等 7 类车身 ID → data_process_*() → can_publish_stat()
      └─ default:
           uds_rx_entry(can_id, data, len)   (app/can/uds/uds_rx_entry.c)
              ├─ can_id == 0x18FF5858 ? → g_force_ota_cmd = data[0]   （裸帧，不进 ISOTP）
              ├─ can_id == 0x18FF5555 ? → 测试用 SWDT 喂狗开关
              ├─ is_uds_id() ? → isotp_receive_frame() → uds_receive_handler()
              └─ 其他 → 丢弃
```

### 2.3 发送数据流

```
UDS 响应 → isotp_send_message() → CanIf_Send()  （入 32 帧 TX 队列）
  → can_module 的 PTB TX 完成中断 → can_module_irq_handler()
      → 调用 can_register_tx_callback 注册的 CanIf_TxCompleteCallback()
          → 从队列取下一帧 → can_transmit_ext/std()
```

> 这就是为什么 CAN HAL 必须新增 **TX 完成回调**：UDS 多帧发送需要"发完一帧再发下一帧"的流控，原代码没有这个回调。

---

## 三、修改 / 新增文件总表

### 3.1 修改的已有文件（12 个）

| 文件 | 改动摘要 |
|------|----------|
| `MDK\template - 副本.uvprojx` | IROM1 0x0/0x80000 → 0x1A000/0x2A000；Include 加 `..\app\can\uds`；新增 `uds` 源文件组（9 个 .c，两个 Target 都要加）；After-build 生成 .bin |
| `MDK\startup_hc32f460.s` | Stack `0x2000`→`0x9000`（36KB）、Heap `0x2000`→`0x4000`（16KB） |
| `config\sys_config.h` | 加 `#define SYS_ENABLE_UDS 1` |
| `startup\hc32f4xx_conf.h` | `LL_SWDT_ENABLE` OFF→ON、`LL_ICG_ENABLE` ON→OFF |
| `app\can\can_module.h` | 精简为纯 HAL；加 TX 回调/忙查询/统一中断声明；协议定义全部移出 |
| `app\can\can_module.c` | 加 TX 回调机制、`m_bTxBusy`、`m_pRxCache`、`can_module_irq_handler()` |
| `app\can\can_hw.c` | 中断类型加 `CAN_INT_PTB_TX`；回调改 `can_module_irq_handler`；去掉 `app_can.h` 依赖 |
| `app\can\app_can.h` | include 改 `can_protocol.h`；加 `extern can_handle_t can_handle;`；删 `app_can_int_callback` 声明 |
| `app\can\app_can.c` | 删 `app_can_int_callback()` 实现；加 UDS include / default 转发 / CanIf_Init / UdsOta_Init；编码 UTF-16LE→UTF-8 |
| `startup\main.c` | 加 UDS include、`UdsOta_App_CheckPendingAck()`、`UdsOta_Poll()`、SWDT 门控 |
| `drivers\...\Source\system_hc32f460.c` | 加 `#include "memory_map.h"`，`VECT_TAB_OFFSET` 自动取 `APP1_START_ADDR=0x1A000` |
| `drivers\drv\drv_mcu_flash.c` | 删本地 `#define STORAGE_SECTOR_ADDR 0x7C000` 等，改 `#include "memory_map.h"`（参数存储移到 0x6E000，避开 boot 跳转槽） |

### 3.2 新增文件（app/can/uds/ 共 9 个 .c + 14 个 .h）

| 文件 | 作用 | 来源 |
|------|------|------|
| `uds_rx_entry.c/h` | UDS RX 入口：CAN ID 过滤 → ISOTP；0x18FF5858 强制指令分支 | 新建（四驱专用） |
| `adapter_can.c/h` | CAN 适配层：32 帧 TX 队列 + Bus-Off 恢复 + 16 个 RX 过滤器 | 从 OTA 移植 + 适配 |
| `can_adapter.h` | ISOTP→CanIf 薄兼容层（inline 包装） | 从 OTA 移植 |
| `isotp_transport.c/h` | ISO 15765-2 传输层：多帧重组/拆分/流控/超时 | 从 OTA 移植 |
| `uds_diagnostic.c/h` | UDS 诊断服务：SID 调度、会话/安全状态机、0x31 例程 | 从 OTA 移植 |
| `uds_did_rid.h` | DID/RID 宏定义 | 从 OTA 移植 |
| `uds_dl_if.h` | 下载接口抽象（函数指针表） | 从 OTA 移植 |
| `uds_dl_bridge.c` | UDS→FlashDownload 桥接 | 从 OTA 移植 |
| `flash_download.c/h` | Flash 下载（EFM_* 直写，地址越界检查） | **重写**（原 OTA 用 FlashAdvanced，四驱无此层） |
| `security_access.c/h` | Seed/Key 安全访问（CRC8 算法） | 从 OTA 移植 |
| `uds_ota.c/h` | 应用封装层：`UdsOta_Init/Poll/App_CheckPendingAck`、`g_force_ota_cmd` | 移植 + 精简 |
| `bootloader_app.c/h` | UDS 共享状态（0x10000 扇区读写）+ Phase 3 补发 ACK + 强制指令宏 | 新建（四驱专用） |
| `common.h` | 公共头（兼容 shim，统一拉入标准库 + hc32_ll.h） | 移植 |
| `hz_timer.h` | 兼容 shim：`#include "sys_tick.h"` | 新建 |
| `memory_map.h` | **Flash 分区唯一宏定义源** | 新建 |

> 说明：实际加入 Keil 工程编译的 **9 个 .c**：`adapter_can.c`、`bootloader_app.c`、`flash_download.c`、`isotp_transport.c`、`security_access.c`、`uds_diagnostic.c`、`uds_dl_bridge.c`、`uds_ota.c`、`uds_rx_entry.c`。

---

## 四、详细修改步骤

### 步骤 0：确定 Flash 分区（先分区，再动代码）

四驱工程当前分区（`app/can/uds/memory_map.h`，与 OTA 工程 boot 保持一致）：

| 区域 | 地址范围 | 大小 | 说明 |
|------|----------|------|------|
| Bootloader | `0x00000 ~ 0x0BFFF` | 48KB | **OTA 工程 boot**（四驱工程不编译，镜像实测 46.55KB） |
| （保留） | `0x0C000 ~ 0x0FFFF` | 16KB | 未分配（boot 与共享区之间的空隙） |
| UDS 共享状态 | `0x10000` | 8KB | Phase1/3 与 boot 通信（`stc_uds_shared_t`） |
| **APP1（本工程链接区）** | `0x1A000 ~ 0x43FFF` | 168KB | 扇区 13~33 |
| **APP2（OTA 下载目标）** | `0x44000 ~ 0x6DFFF` | 168KB | 扇区 34~54 |
| 参数存储 | `0x6E000` | 8KB | 扇区 55，四驱 APP 自有系统（`drv_mcu_flash.c`） |
| Boot 跳转槽 | `0x7C000` | 8KB | 扇区 62，boot 使用，**APP 勿写** |

> 历史：初次移植是 80KB/80KB（APP1=0x1A000、APP2=0x4C000），2026-08-04 扩展到 168KB/168KB（APP2 改到 0x44000）。当前代码为 168KB 版本。

### 步骤 1：Keil 工程配置（`MDK\template - 副本.uvprojx`）

> 当前四驱工程的实际工程文件是 `template - 副本.uvprojx`（`template.uvprojx` 为未改动原版）。**Debug / Release 两个 Target 都要改。**

1. **IROM1（链接地址，两个 Target 都要改）**：
   ```
   OCR_RVCT4:  StartAddress  0x0      → 0x1a000
               Size          0x80000  → 0x2a000
   ```
   - 若你的工程已经是 0x1A000/0x14000（80KB 版），只需把 Size 改为 `0x2A000`。
2. **Include Paths**：追加 `..\app\can\uds`
3. **新增源文件组**：新建 Group `uds`，加入 9 个 .c（见 3.2 表）。
4. **After-build 生成 .bin**（OTA 下载需要 bin 镜像）：
   ```
   fromelf.exe --bin --output ".\output\debug\@L.bin" ".\output\debug\@L.axf"
   ```

### 步骤 2：启动文件（`MDK\startup_hc32f460.s`）

```
Stack_Size   EQU  0x00002000  →  0x00009000   ; 36KB
Heap_Size    EQU  0x00002000  →  0x00004000   ; 16KB
```
原因：UDS 协议栈（ISOTP 多帧重组 + 诊断 + 下载）调用深度大，且 `flash_download.c` 有大块暂存 buffer，需要足够栈空间。

### 步骤 3：功能宏开关

`config\sys_config.h`（`SYS_ENABLE_CAN` 下面加一行）：
```c
#define SYS_ENABLE_CAN    1     // can模块
#define SYS_ENABLE_UDS    1     // UDS 诊断协议栈   ← 新增
```

`startup\hc32f4xx_conf.h`：
```c
#define LL_SWDT_ENABLE   (DDL_OFF) → (DDL_ON)    // 使能硬件看门狗（boot 用 SWDT 计数判定坏块）
#define LL_ICG_ENABLE    (DDL_ON)  → (DDL_OFF)
```

### 步骤 4：CAN HAL 层增强（UDS 多帧发送的前提）

#### 4.1 `app\can\can_module.h`
- 删除 `#include <string.h>`、`#include <stdio.h>`，保留 `#include <stdint.h>`，新增 `#include <stdbool.h>`；
- 删除全部协议/业务定义（移到 `can_protocol.h`）：`can_cmd_t`、`gear_status_t`、`calib_support_t`、`awd_cal_enable_t`、`machine_type_spc_t`、`cal_stat_t`、`run_stat_t`、`fault_stat_t`、`GET_SPEED_*` 宏、`can_18FF9DF0_t`~`can_18FF0104_t`、`can_rx_data_t`/`can_tx_data_t`、`motor_state_t`、`can_rx_message_t`；
- 删除过时声明：`can_transmit_polling`、`can_text`、`CanConfig`；
- **新增 4 个声明**（文件末尾）：
```c
/* TX 完成回调函数类型 */
typedef void (*can_tx_callback_t)(void);
void can_register_tx_callback(can_tx_callback_t pfnCallback);  /* 中断上下文调用 */
bool can_is_tx_busy(void);                                     /* 发送忙查询 */
void can_module_irq_handler(void);                             /* 统一中断入口 */
```

#### 4.2 `app\can\can_module.c`
- 新增模块静态变量：
```c
static can_tx_callback_t m_pfnTxCallback = NULL;
static volatile bool     m_bTxBusy       = false;
static can_rx_cache_t   *m_pRxCache      = NULL;
```
- `can_module_init()` 内新增两行：
```c
m_pRxCache = &handle->can_rx;                       /* 保存 RX 缓存指针，供中断快速访问 */
memset(&handle->can_rx, 0, sizeof(can_rx_cache_t)); /* 清零 RX 缓存 */
```
- `can_transmit_std()` / `can_transmit_ext()`：在 `CAN_StartTx(...)` 之后加 `m_bTxBusy = true;`
- **新增 `can_module_irq_handler()`**（把原 `app_can.c` 里的 `app_can_int_callback()` 逻辑搬进来，并增加 TX 完成处理）：
```c
void can_module_irq_handler(void)
{
    CM_CAN_TypeDef *CANx = CM_CAN;
    uint32_t status = 0;
    /* RX 接收：硬件 FIFO → 软件环形缓存 */
    if (CAN_GetStatus(CANx, CAN_FLAG_RX) == SET) {
        status |= CAN_FLAG_RX;
        stc_can_rx_frame_t frame;
        while (CAN_GetRxBufStatus(CANx) != CAN_RX_BUF_EMPTY) {
            while (CAN_GetRxFrame(CM_CAN, &frame) == LL_OK) {
                if (m_pRxCache != NULL) can_rx_cache_put(m_pRxCache, &frame);
            }
        }
    }
    if (CAN_GetStatus(CANx, CAN_FLAG_RX_OVERRUN) == SET)  status |= CAN_FLAG_RX_OVERRUN;
    if (CAN_GetStatus(CANx, CAN_FLAG_RX_BUF_FULL) == SET) status |= CAN_FLAG_RX_BUF_FULL;
    if (CAN_GetStatus(CANx, CAN_FLAG_RX_BUF_WARN) == SET) status |= CAN_FLAG_RX_BUF_WARN;
    /* ★ TX 完成：清忙标志 + 回调（UDS 多帧发送依赖） */
    if (CAN_GetStatus(CANx, CAN_FLAG_PTB_TX) == SET) {
        status |= CAN_FLAG_PTB_TX;
        m_bTxBusy = false;
        if (m_pfnTxCallback != NULL) m_pfnTxCallback();
    }
    if (CAN_GetStatus(CANx, CAN_FLAG_STB_TX) == SET) status |= CAN_FLAG_STB_TX;
    /* 错误 / Bus-Off 恢复 */
    if (CAN_GetStatus(CANx, CAN_FLAG_ERR_INT) == SET) {
        if (CAN_GetStatus(CANx, CAN_FLAG_BUS_OFF) == SET) {
            CAN_ExitLocalReset(CANx);
            status |= CAN_FLAG_BUS_OFF;
        }
        status |= CAN_FLAG_ERR_INT;
    }
    if (CAN_GetStatus(CANx, CAN_FLAG_BUS_ERR) == SET) status |= CAN_FLAG_BUS_ERR;
    if (status != 0) CAN_ClearStatus(CANx, status);
}
```

#### 4.3 `app\can\can_hw.c`
```c
.can_int_type = (CAN_INT_RX | CAN_INT_RX_OVERRUN | CAN_INT_RX_BUF_FULL |
                 CAN_INT_RX_BUF_WARN | CAN_INT_ERR_INT)
             → (… | CAN_INT_PTB_TX),          // ★ 必须使能 TX 完成中断
.can_int_callback = app_can_int_callback
             → can_module_irq_handler,        // ★ 统一中断入口
// 删除 #include "app_can.h"
```

#### 4.4 `app\can\app_can.c`
- **删除 `app_can_int_callback()` 整个函数**（约 70 行，原文件末尾）；
- 文件编码 UTF-16LE → UTF-8（避免 Keil 与 git 乱码）。

### 步骤 5：CAN 三层拆分

目的：`can_module.h` 成为**纯 HAL**（只依赖 `hc32_ll.h` + `<stdint.h>` + `<stdbool.h>`），bootloader 可以只复制 3 个文件（`can_module.h/c`、`can_hw.c`）独立收发 CAN 帧，不被电机状态机/档位等业务定义污染。

- **新增 `app\can\can_protocol.h`**：把旧 `can_module.h` 里的协议层全部内容搬入——`can_cmd_t`/`gear_status_t`/`run_stat_t`/`fault_stat_t` 等枚举、`can_18FF9DF0_t`~`can_18FF0104_t` 各 ID 结构体、`can_rx_data_t`/`can_tx_data_t` 联合体、`GET_SPEED_FROM_FDF0/31F9` 宏。`#include "can_module.h"` 作为其依赖。
- **`app\can\can_module.h`**：按步骤 4.1 精简。
- **`app\can\app_can.h`**：
  - `#include "can_module.h"` → `#include "can_protocol.h"`（协议层自动拉入 HAL 层）；
  - 新增 `extern can_handle_t can_handle;`（供 UDS 适配层 `CanIf_Init(&can_handle)` 引用）；
  - `motor_state_t` 定义移入本文件（业务聚合）；
  - 删除 `void app_can_int_callback(void);` 声明。

### 步骤 6：新增 UDS 协议栈（`app\can\uds\`）

把 3.2 表的 23 个文件整体拷贝进工程，9 个 .c 加入 Keil 编译。从 OTA 源工程移植时**必须做的适配**（对照 `uds移植.md` 第五节）：

| OTA 原版 | 四驱适配 | 原因 |
|----------|---------|------|
| `tickTimer_GetCount()` | `SysTick_GetTick()` | 四驱已有 1ms SysTick |
| `NonBlockingDelay_t` | `uint32_t` + 差值比较 | 减少依赖 |
| `flash_advanced.h` / `FlashAdv_*` | `hc32_ll_efm.h` / `EFM_*` | 四驱没有 flash_advanced 层 |
| `en_result_t` / `Ok` | `int32_t` / `0` | 四驱 EFM 返回 `int32_t` |
| `CanIf_Init(void)` | `CanIf_Init(can_handle_t *handle)` | 共享 `app_can.c` 的 `can_handle` |
| `Boot_SetRunSlotToAddr` | 注释掉 | 四驱 APP 不写跳转槽（boot 负责） |
| `hz_timer.h` | 保留为 shim（`#include "sys_tick.h"`） | ISOTP 依赖该头 |
| `rtt_log.h` + `LOG_CH` | 原样保留 | 四驱 `service/RTT/` 有相同宏 |

**关键文件要点**：
- `flash_download.c`：`FlashDownload_Init()` 里 `max_firmware_size = FW_APP_MAX_SIZE`、`user_start_addr = FW_APP_START_ADDR`、`user_end_addr = FW_APP_START_ADDR + FW_APP_MAX_SIZE - 1`，并做 `m + size` 越界检查（防止擦到参数区/跳转槽）。
- `bootloader_app.c`：`UdsShared_Write/Read/Clear` 直接对 `UDS_SHARED_SECTOR_BASE (0x10000)` 做扇区擦除 + 字编程；`App_CheckPendingUdsAck()` 读到 `pending_sid == 0x11` 时经 `CanIf_Send()` 补发 `04 51 01` 并清共享区（Phase 3）。
- `uds_diagnostic.c` 的 `uds_handle_routine_control(0x31)`：安全解锁后写共享区（`phase=1, pending_sid=0x31`）→ `g_delayed_reset_ms = DELAYED_RESET_MS`（100ms 后软复位，Phase 1）。

### 步骤 7：`app\can\app_can.c` 串联 UDS（3 处改动）

**改动 1**——文件头新增 include（当前第 9~13 行）：
```c
#if SYS_ENABLE_UDS
#include "uds/uds_rx_entry.h"
#include "uds/uds_ota.h"
#include "uds/adapter_can.h"
#endif
```
（同时第 37 行 `can_handle_t can_handle;` 保持全局可见，供 `CanIf_Init` 引用。）

**改动 2**——`app_can_receive()` 的 `default:` 分支（当前第 156~160 行）：
```c
        default:
#if SYS_ENABLE_UDS
            uds_rx_entry(rx_mess.can_id, rx_mess.data.rx_data, rx_mess.len);  // ★ 未匹配车身 ID → UDS
#endif
            return;  // 未知 ID，不发布
```

**改动 3**——`app_can_init()`（当前第 357~363 行）：
```c
void app_can_init(void)
{
    can_module_init(&can_handle, &CAN_HW);
#if SYS_ENABLE_UDS
    CanIf_Init(&can_handle);   // ★ 注册 TX 完成回调 + 初始化 TX 队列
    UdsOta_Init();             // ★ 初始化 ISOTP / UDS / FlashDownload
#endif
    //根据配置信息对报文进行使能
}
```
> 注意：`app_can_task()` **不要再调用 `UdsOta_Poll()`**（历史版本加过，2026-08-02 已删除，否则与 main 重复调用）。

### 步骤 8：`startup\main.c`（4 处改动，当前行号）

**改动 1**——文件头（第 27~29 行）：
```c
#if SYS_ENABLE_UDS
#include "../app/can/uds/uds_ota.h"
#endif
```

**改动 2**——`main()` 开头（第 99 行）：
```c
MAIN_D("===== main(): app1 =====\r\n");   // 第一行打印，确认 APP 在跑
```

**改动 3**——`System_Init()` 之后（第 114~116 行）：
```c
#if SYS_ENABLE_UDS
    UdsOta_App_CheckPendingAck();  // Phase 3: 检查并补发 UDS 挂起响应 (51 01)
#endif
```

**改动 4**——`while(1)` 循环（第 121~133 行）：
```c
    while (1) {
#if SYS_ENABLE_UDS
        if (g_swdt_feed_disable == 0U) {   // 测试用：0x18FF5555 可临时停止喂狗
            SWDT_FeedDog();
        }
#else
        SWDT_FeedDog();
#endif
        uint32_t now = SysTick_GetTick();
        Sys_Schedule_Run();  // 调度器运行（内含 app_can_task：收/发/发布）
#if SYS_ENABLE_UDS
        UdsOta_Poll();       // ★ UDS/ISOTP/延迟复位/强制指令轮询（仅此一处）
#endif
    }
```

### 步骤 9：向量表偏移 + 参数存储地址

`drivers\cmsis\Device\HDSC\hc32f4xx\Source\system_hc32f460.c`：
```c
#include "system_hc32f460.h"
#include "memory_map.h"          // ← 新增（路径需在 Include 里，或写相对路径）
```
- `VECT_TAB_OFFSET` 未定义时默认 `0x0`，现在由 `memory_map.h` 定义为 `APP1_START_ADDR (0x1A000)`，`SystemInit()` 里 `SCB->VTOR = VECT_TAB_OFFSET` 自动生效，**不需要在 main.c 里改 VTOR**。

`drivers\drv\drv_mcu_flash.c`：
```c
// 删除这三行本地宏定义：
// #define FLASH_SECTOR_SIZE   8192U
// #define STORAGE_SECTOR_ADDR 0x7C000UL
// #define STORAGE_MAGIC       0x5AA55AA5UL
#include "memory_map.h"          // ← 新增
```
> ⚠️ 原因（见 `偏移bug.md`）：原 `STORAGE_SECTOR_ADDR = 0x7C000` 与 boot 的 `APP_RUN_SLOT_ADDR` **地址冲突**——每次 boot 启动写跳转槽会擦掉四驱参数存储，导致 APP 每次启动都触发擦除，且 `flash_unlock/lock` 的中断宏行为颠倒会永久关中断、SysTick 停摆。加 OTA 后必须把参数存储挪到 `0x6E000`（memory_map.h 已定义）。

### 步骤 10：阶段 2/3 强制 OTA 指令（`四驱APPOTA优化.md`）

**协议**：CAN ID `0x18FF5858`，data[0]：
- `0xFF` → 强制进入 bootloader 编程模式
- `0x01` → 强制下次启动 APP1
- `0x02` → 强制下次启动 APP2

**改动 1**——`bootloader_app.h` 新增宏：
```c
#define BOOT_FORCE_CMD_CAN_ID   0x18FF5858UL
#define BOOT_FORCE_CMD_ENTER_BL 0xFFU
#define BOOT_FORCE_CMD_BOOT_APP2 0x02U
#define BOOT_FORCE_CMD_BOOT_APP1 0x01U
#define BOOT_FORCE_CMD_WINDOW_MS 50U
```

**改动 2**——`uds_rx_entry.c` 在 `uds_rx_entry()` 最前面加分支（裸帧，不进 ISOTP）：
```c
if (can_id == BOOT_FORCE_CMD_CAN_ID) {
    if (len >= 1U) g_force_ota_cmd = data[0];
    return;
}
```

**改动 3**——`uds_ota.h/c`：
```c
// uds_ota.h
extern volatile uint8_t g_force_ota_cmd;
// uds_ota.c 定义 + UdsOta_Poll() 最前面：
volatile uint8_t g_force_ota_cmd = 0;

if (g_force_ota_cmd != 0U) {
    uint8_t u8ForceCmd = g_force_ota_cmd;
    g_force_ota_cmd = 0U;                       // 先清零，防 SRAM 保留残留
    MAIN_D("Force OTA cmd 0x18FF5858 = 0x%02X, resetting to bootloader...\r\n", u8ForceCmd);
    NVIC_SystemReset();                          // 交 boot 50ms 窗口按指令语义处理
    while (1) { }
}
```

### 步骤 11：OTA 三阶段流程（确认整体逻辑）

```
Phase 1 (APP):  10 03 → 27 (Seed/Key) → 31
                → 写共享区 0x10000 (phase=1, pending_sid=0x31)
                → g_delayed_reset_ms=100 → UdsOta_Poll() 倒计时 → NVIC_SystemReset()
Phase 2 (Boot): 读到 phase==1 → 补发 31 ACK → UDS 下载 (10 02 → 27 → 34 → 36×N → 37)
                → 写共享区 (phase=2, result) → 等 0x11 → 复位
Phase 3 (APP):  启动时 App_CheckPendingUdsAck() 读到 pending_sid==0x11
                → CanIf_Send 补发 51 01 → UdsShared_Clear() → OTA 完成
```

---

## 五、移植常见坑（来自实测修复记录）

| # | 现象 | 原因 | 修复 |
|---|------|------|------|
| 1 | `0x36/0x37` 永远回 `7F xx 78`（pending），不回 `76/77` | 移植时在 `FlashDownload_OnTransferData()` 末尾误加了 `g_ctx.pending_response = true;` | 删除该行（同步写 Flash 不需要延迟响应） |
| 2 | `UdsOta_Poll()` 被调用两次 | main 和 `app_can_task()` 各调一次 | 删 `app_can_task()` 里的调用，只留 main 一处 |
| 3 | Phase 1 复位前触发过流故障、主循环卡死 | 0x31 handler 里的 PB6 闪烁恰好是**电机2 下管 PWM 引脚** | 删除 `uds_diagnostic.c` 0x31 handler 的 PB6 闪烁块 |
| 4 | APP2 窗口过小 / 越界擦除参数区和跳转槽 | `max_firmware_size` 默认 256KB | `flash_download.h` 用 `FW_APP_MAX_SIZE`（168KB），`flash_download.c` 加 `m+size` 越界检查 |
| 5 | 启动后 SysTick 停摆、系统"死" | `drv_mcu_flash.c` 参数存储 0x7C000 与 boot 跳转槽冲突；`flash_unlock/lock` 中断宏顺序颠倒 | 存储挪到 0x6E000（见步骤 9）；建议顺手修正 `INTERRUPT_DISABLE/ENABLE` 配对 |
| 6 | OTA 后无法跳回 APP | OTA boot 的 `UDS_POST_FLASH_BOOT_ADDR` 不对 | 四驱 boot 配置为 `APP1_START_ADDR`，升级完成跳回 APP1 |
| 7 | 强制指令检测不到 | 0x18FF5858 裸帧被当 ISOTP 单帧解析成错误 SID 丢弃 | 在 `uds_rx_entry()` 里 ISOTP 之前单独分支处理 |

---

## 六、验证清单

- [ ] 四驱工程 Keil 编译通过（ARMCC V5.06 update 7），无 L6218/溢出错误
- [ ] 生成的 .bin 位于 `MDK\output\debug\@L.bin`
- [ ] IROM1 = `0x1A000 / 0x2A000`，`MAP` 文件确认入口在 0x1A000
- [ ] `SystemInit()` 后 `SCB->VTOR == 0x1A000`（RTT 打印 `===== main(): app1 =====` 即确认在跑）
- [ ] 未匹配车身 ID 的帧能进 `uds_rx_entry()`（断点：`if (!is_uds_id(can_id)) return;`）
- [ ] UDS 4 个 ID 能进 ISOTP（断点：`isotp_receive_frame()` 首行）
- [ ] `10 03 → 27 → 31` 触发 Phase 1（打印 `Delayed reset scheduled in 100 ms`）
- [ ] boot 补发 31 ACK → 0x34 传 168KB → 0x36 672 块（序号回绕 OK）→ 0x37 CRC 通过
- [ ] 擦除范围不越过 `0x6E000`（不碰扇区 55 参数存储、扇区 62 跳转槽）
- [ ] 复位后按实际烧录槽位跳转（APP2 或 APP1）
- [ ] 0x18FF5858 / 0xFF|0x01|0x02 能强制复位进 boot（阶段 2/3）

---

## 七、参考文档

| 文档 | 位置 |
|------|------|
| CAN 模块增补记录 | `app\can\can_update.md` |
| CAN 模块三层拆分说明 | `app\can\CAN模块拆分说明.md` |
| UDS 协议移植说明 | `app\can\uds\uds移植.md` |
| 四驱 APP OTA 优化（阶段2/3） | `app\can\uds\四驱APPOTA优化.md` |
| 四驱新 APP 分区 | `app\can\uds\四驱新APP分区.md` |
| CAN+UDS+OTA 代码分析报告 | `project\app1\OTA问题.md` |
| 偏移（中断/SysTick/存储冲突）bug 分析 | `project\app1\偏移bug.md` |
