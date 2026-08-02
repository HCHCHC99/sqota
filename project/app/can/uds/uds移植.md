# UDS 协议移植说明

> 日期：2026-07-24  
> 基线：`uds-port-base` 分支（commit `0053485`，"修改can"）  
> 源工程：`D:\ota_ddl3.3_v.3.1\merge_v.1.0.0\app1\projects\ev_hc32f460_lqfp100_v2`（OTA APP）  

---

## 一、原有四驱控制器 CAN 协议

### 1.1 三层架构

```
app_can.h/c        ← 应用层：电机状态机、消息收发调度、业务发布
can_protocol.h     ← 协议层：CAN ID 位域结构体、枚举
can_module.h/c     ← HAL 层：GPIO/波特率/滤波器/中断/收发原语
can_hw.c           ← 硬件配置常量实例
```

### 1.2 车身 CAN ID（7 类）

| CAN ID | 用途 | 处理函数 |
|--------|------|---------|
| `0x18FF9DF0` / `0x18FFC060` | 电机控制及状态 | `data_process_9DF0()` |
| `0x18FF8018` | 是否支持一键标定 | `data_process_8018()` |
| `0x18FFEC18` | 温度标定使能 | `data_process_EC18()` |
| `0x18FF17F0` | 档位状态 | `data_process_17F0()` |
| `0x18FFFDF0` | 车速（通用） | `data_process_FDF0()` |
| `0x18FF31F9` | 车速（盛硕） | `data_process_31F9()` |
| `0x18FF8818` / `0x18FFFFF0` | 机型选择 | `data_process_8818()` |

### 1.3 接收流程（原始）

```c
app_can_receive()
  → can_read() 从环形缓存取一帧
    → switch (can_id):
        case 0x18FFxxxx → data_process_*() → can_publish_stat()
        default → return  // ← 未知 ID 直接丢弃
```

---

## 二、UDS 协议串联方案（方案 B：最小侵入）

### 2.1 核心思路

在 `app_can_receive()` 的 `default` 分支插入 UDS 入口。车身协议优先匹配 7 类 `0x18FFxxxx`，未匹配的帧交给 UDS 层判断，UDS 也不处理则丢弃。

```
CAN 帧到达
  → can_module ISR → 环形缓存
    → app_can_receive()
        ├─ switch 匹配 0x18FFxxxx → data_process_*() → 车身协议发布
        └─ default → uds_rx_entry()
             ├─ is_uds_can_id()? → isotp_receive_frame() → uds_receive_handler()
             │    └─ UDS 响应 → isotp_send_message() → CanIf_Send() → can_transmit_ext()
             └─ 不是 UDS ID → 丢弃
```

### 2.2 为什么选择方案 B

- 车身协议代码一行不动
- RX 只走一条路径，无竞争
- TX 共享 `can_handle_t`，UDS 通过 `CanIf_Send()` 发送
- 后续添加新的车身 CAN ID 只需在 switch 中加 case

---

## 三、新增 UDS 协议栈

### 3.1 分层层级

```
uds_ota.c/h            ← 应用封装层：UdsOta_Init / Poll / App_CheckPendingAck
uds_diagnostic.c/h      ← UDS 诊断服务：SID 调度、会话/安全状态机
isotp_transport.c/h     ← ISO 15765-2 传输层：多帧重组/拆分
uds_dl_bridge.c         ← UDS→FlashDownload 桥接（接口表模式）
flash_download.c/h      ← Flash 下载模块（EFM_* 直操作）
uds_dl_if.h             ← 下载接口抽象
uds_did_rid.h           ← DID/RID 定义
security_access.c/h     ← Seed/Key 安全访问
adapter_can.c/h         ← CAN 适配层：TX 发送队列 + Bus-Off 恢复
can_adapter.h           ← ISOTP→CanIf 薄兼容层（inline）
bootloader_app.c/h      ← UDS 共享状态（0x10000 扇区）+ Phase 3 延迟 ACK
uds_rx_entry.c/h        ← UDS RX 入口：CAN ID 过滤 → ISOTP
common.h                ← 公共头
```

### 3.2 关键 UDS CAN ID

| CAN ID | 方向 | 用途 |
|--------|------|------|
| `0x18DA03F1` | TBOX → ECU | 物理寻址请求 |
| `0x18DAF103` | ECU → TBOX | 物理寻址响应 |
| `0x18DBFFF0` | 广播 | 功能寻址请求 |
| `0x18FF8118` | TBOX → ECU | OTA 专用 |

---

## 四、修改的现有文件

| 文件 | 改动 |
|------|------|
| `app/can/can_hw.c` | `.can_int_type` 加 `CAN_INT_PTB_TX`（TX 完成中断使能） |
| `app/can/app_can.c` | `app_can_init` 加 `CanIf_Init(&can_handle)` + `UdsOta_Init()`；`default:` 加 `uds_rx_entry()`；`app_can_task` 原加 `UdsOta_Poll()`（**2026-08-02 已删除**，仅 main 调用） |
| `app/can/app_can.h` | 加 `extern can_handle_t can_handle`（供 UDS 适配层引用） |
| `startup/main.c` | 加 `UdsOta_App_CheckPendingAck()`、`while(1)` 中 `UdsOta_Poll()`；注：`SCB->VTOR` 不在 main.c，由 `system_hc32f460.c` 的 `VECT_TAB_OFFSET=0x1A000` 在 `SystemInit()` 中自动设置 |
| `config/sys_config.h` | 加 `#define SYS_ENABLE_UDS 1` |
| `MDK/startup_hc32f460.s` | Stack `0x2000→0x9000`（36KB），Heap `0x2000→0x4000`（16KB） |

---

## 五、从 OTA 移植的关键适配

| OTA 原版 | 四驱适配 | 原因 |
|----------|---------|------|
| `tickTimer_GetCount()` | `SysTick_GetTick()` | 四驱有 SysTick，无需 TickTimer |
| `NonBlockingDelay_t` | `uint32_t` + 差值比较 | 减少依赖 |
| `flash_advanced.h` / `FlashAdv_*` | `hc32_ll_efm.h` / `EFM_*` | 四驱无 flash_advanced，直接调用 EFM |
| `rtt_log.h` + `LOG_CH` | **保持原样** | 四驱 `service/RTT/rtt_log.h` 有相同宏 |
| `MAIN_D/I/W/E` | **保持原样** | 同上 |
| `en_result_t` / `Ok` | `int32_t` / `0` | 四驱 EFM 返回 `int32_t` |
| `LL_PERIPH_EFM` | 需 `#include "hc32_ll.h"` | 宏定义在 `hc32_ll.h` |
| `CanIf_Init(void)` | `CanIf_Init(can_handle_t *handle)` | 共享 `app_can.c` 的 `can_handle` |
| `Boot_SetRunSlotToAddr` | 注释掉（APP-only） | 四驱无 bootloader |
| `hz_timer.h` | 删除 | 仅 OTA 有，功能已由 `sys_tick.h` 覆盖 |

---

## 六、main.c 启动流程

```
复位后（startup → SystemInit，自动执行）:
  SCB->VTOR = VECT_TAB_OFFSET = 0x1A000   ← OTA 对齐（system_hc32f460.c 定义）

main():
  MAIN_D("===== main(): app1 =====")
  LL_PERIPH_WE(...)
  BSP_CLK_Init()                         ← 四驱硬件时钟（MPLL 200MHz）
  SysTick_Init(1000U)                    ← 1ms 时基
  __enable_irq()
  System_Init()                          ← 内含 app_can_init → CanIf_Init + UdsOta_Init
  UdsOta_App_CheckPendingAck()           ← Phase 3: 补发 51 01 ACK（读 0x10000 扇区）
  LL_PERIPH_WP(...)
  while(1):
    SWDT_FeedDog()
    Sys_Schedule_Run()                   ← 调度器（app_can_task：收/发/发布）
    UdsOta_Poll()                        ← Phase 1: 延迟复位倒计时 + ISOTP/UDS/CAN 轮询（2026-08-02 起仅此处调用）
```

---

## 七、UDS OTA 三阶段（Phase 1/2/3）

| 阶段 | 运行固件 | 触发 | 动作 |
|------|---------|------|------|
| **Phase 1** | APP | 收到 0x31（RoutineControl） | 写 `0x10000` 共享 Flash → 设置 `g_delayed_reset_ms=100` → `UdsOta_Poll()` 倒计时 → `NVIC_SystemReset()` |
| **Phase 2** | Bootloader | 读到 `phase==1` | 补发 31 ACK → UDS 下载（`10 02` → `27` → `34` → `36×N` → `37`）→ 写 `phase=2, result=1` → 等 `0x11` → 复位 |
| **Phase 3** | APP（重新启动） | 读到 `pending_sid==0x11` | 发送 `51 01` ACK → 擦除 `0x10000` → OTA 完成 |

> **注意**：四驱控制器本身不编译 bootloader，Phase 2 由 **OTA 工程的 boot** 完成（烧录在 `0x0`）。2026-08-02 已实测 **Phase 1 → 2 → 3 全链路通过**。

---

## 八、Flash 布局

| 地址 | 大小 | 用途 |
|------|------|------|
| `0x00000000` | 48KB（0~0xC000） | Bootloader（使用 **OTA 工程的 boot**，镜像实测 46.55KB） |
| `0x00010000` | 8KB | **UDS 共享状态**（`stc_uds_shared_t`，56字节） |
| `0x0001A000` | 80KB（0x1A000~0x2DFFF） | APP1（当前固件，2026-08-02 由 200KB 调整为 80KB） |
| `0x0004C000` | 80KB（0x4C000~0x5FFFF） | APP2（OTA 下载目标，同上调整为 80KB） |
| `0x0006E000` | 8KB | 参数存储（`drv_mcu_flash.c` 的 `STORAGE_SECTOR_ADDR`，**APP 自有系统**） |
| `0x0007C000` | 8KB | **Bootloader 跳转槽 `APP_RUN_SLOT`**（OTA boot 使用，勿占用） |

---

## 九、调试打印

### 9.1 开启方式

`bootloader_app.h` 中定义了三个开关：

```c
#define UDS_DEBUG  1   // UDS 诊断层：[UDS] 标签，青色/绿色/黄色/红色
#define ISO_DEBUG  1   // ISOTP 传输层：[ISOTP] 标签
#define OTA_DEBUG  1   // CAN ID 帧级打印：[OTA] 标签，seq/time/direction/8 bytes
```

另有始终开启的：
- `CANIF_D/I/W/E` → `[CANIF]` 标签（CAN 适配层）
- `FW_D/I/W/E` → `[FW]` 标签（Flash 下载）
- `DL_D/I/W/E` → `[DL]` 标签（下载桥接）
- `MAIN_D/I/W/E` → `[MAIN]` 标签（全局阶段打印）

### 9.2 输出示例

```
[MAIN] === UDS Stack Init Start ===
[CANIF] === CanIf Init Start ===
[CANIF] TX callback registered
[CANIF] === CanIf Init Done ===
[UDS] session_mode=1 (DEFAULT), security_state=0 (LOCKED)
[UDS] === UDS Init Done ===
[MAIN] === UDS Stack Init Done ===
[MAIN] Delayed reset done, resetting...
[OTA] seq=1, time=0.123s, [RX] 0x18DA03F1, 02 10 03 00 00 00 00 00 <-- SF DiagSession
[OTA] seq=2, time=0.125s, [TX] 0x18DAF103, 06 50 03 00 32 01 F4 00 
```

---

## 十、编译注意事项

1. **编译器**：ARMCC V5.06 update 7
2. **启动文件**：Stack `0x9000`（36KB），Heap `0x4000`（16KB）—— UDS 协议栈深度调用 + 60KB Flash 下载 buffer 需要足够栈空间
3. **Include Paths**：需包含 `app/can/uds/`、`service/`、`service/RTT/`、`config/`
4. **源文件**：`app/can/uds/*.c`（共 11 个 .c 文件）需加入工程

---

## 十一、文件清单

```
app/can/
├── can_module.h/c         # HAL 层（不变）
├── can_hw.c               # +CAN_INT_PTB_TX
├── can_protocol.h         # 车身协议（不变）
├── app_can.h/c            # +UDS 串联
└── uds/
    ├── uds_rx_entry.h/c    # 新建：UDS RX 入口
    ├── adapter_can.h/c     # 移植+适配：CAN 适配层
    ├── can_adapter.h       # 移植：ISOTP→CanIf 兼容层
    ├── isotp_transport.h/c # 移植：ISO 15765-2
    ├── uds_diagnostic.h/c  # 移植：UDS 诊断服务
    ├── uds_did_rid.h       # 移植：DID/RID 定义
    ├── uds_dl_if.h         # 移植：下载接口抽象
    ├── uds_dl_bridge.c     # 移植：下载桥接
    ├── flash_download.h/c  # 重写：EFM_* Flash 下载
    ├── security_access.h/c # 移植：Seed/Key
    ├── uds_ota.h/c         # 移植+精简：应用封装层
    ├── bootloader_app.h/c  # 新建：UDS 共享状态+Phase 3
    └── common.h            # 移植：公共头
```

---

## 十二、调试断点

### 确认报文是否传入 UDS 协议判断

**断点 ①：UDS RX 入口**

| 项目 | 值 |
|------|-----|
| 文件 | `app/can/uds/uds_rx_entry.c` |
| 函数 | `uds_rx_entry()` |
| 行 | `if (!is_uds_id(can_id)) return;` |
| 目的 | 确认 CAN 帧到达 UDS 入口 |

打到断点后观察 `can_id` 变量：
- `0x18DA03F1` → UDS 物理寻址请求
- `0x18DAF103` → UDS 物理寻址响应
- `0x18DBFFF0` → UDS 功能寻址广播
- `0x18FF8118` → OTA 专用
- 其他 ID → 非 UDS 帧，`is_uds_id()` 返回 false，被丢弃

**断点 ②：ISOTP 接收**

| 项目 | 值 |
|------|-----|
| 文件 | `app/can/uds/isotp_transport.c` |
| 函数 | `isotp_receive_frame()` |
| 行 | 函数体第一条语句 |
| 目的 | 确认帧进入 ISOTP 传输层 |

### 排查流程

```
断点 ① 触发？
  ├─ 是 → can_id 是 UDS ID？
  │       ├─ 是 → 断点 ② 触发？
  │       │       ├─ 是 → UDS 协议栈正常工作 ✓
  │       │       └─ 否 → isotp_receive_frame() 调用参数有问题
  │       └─ 否 → 非 UDS 帧，正常丢弃
  └─ 否 → app_can_receive() 的 default: 分支没走到
            → 在 app_can.c 的 `default:` 行打断点
            → can_read() 返回 CAN_RET_ERR_NODATA？
                ├─ 是 → 总线上没有 CAN 帧，或 CAN 硬件过滤器丢弃了帧
                └─ 否 → 帧被 switch-case 中某个 case 匹配走了（检查 can_id）
```
---

## 十三、移植实测结果（2026-08-02 通过 ✅）

四驱控制盒工程 OTA 全链路实测通过：

```
APP(四驱)                        OTA Boot(0x0)                       APP(四驱)
  │ 10 03 → 27 → 31                │                                 │
  │ 写共享区 phase=1, pending=0x31  │                                 │
  │ 延迟 100ms → NVIC_SystemReset ──→│ Boot_StartupSequence 读 phase==1│
  │                                 │ → Bootloader_UdsMain (Phase 2)  │
  │                                 │ 补发 31 ACK → 下载 80KB → 0x11  │
  │                                 │ 写 pending_sid=0x11 → 复位 ────→│
  │                                 │                                 │ App_CheckPendingUdsAck → 51 01 (Phase 3)
  │                                 │                                 │ UdsShared_Clear → OTA 完成
```

要点：
- 四驱工程**不编译 bootloader**，Phase 2 直接使用 **OTA 工程（merge_v.1.0.0）的 boot**，烧录在 `0x0`；
- Phase 1 由四驱 APP 完成（0x31 handler → 共享区 → 延迟复位）；
- Phase 3 由四驱 APP 启动时补发 `51 01`；
- OTA boot 的 `UDS_POST_FLASH_BOOT_ADDR = APP1_START_ADDR`，升级完成后始终跳回 APP1。

## 十四、移植后修复记录（2026-08-02）

| # | 问题 | 修复 |
|---|------|------|
| 1 | `0x36/0x37` 永远回 `7F xx 78`（pending），不回 `76/77` | 删除 `flash_download.c` `FlashDownload_OnTransferData()` 末尾的 `g_ctx.pending_response = true;`（OTA 工程本来就没有这一行，属于移植引入） |
| 2 | `UdsOta_Poll()` 在 main 和 `app_can_task` 中被重复调用 | 删除 `app_can.c` `app_can_task()` 中的调用，仅保留 main 循环一处 |
| 3 | Phase 1 的 PB6 闪烁会操作**电机2下管 PWM 引脚**（`pwm_hw.c` CH_M2_LV = PB6），导致过流故障、主循环被卡住、延迟复位无法执行 | 删除 `uds_diagnostic.c` 0x31 handler 中的 PB6 闪烁块；OTA 工程 boot/app1/app2 的 Phase 1/2/3 闪烁和 `ShowBootStatus()` 一并删除 |
| 4 | APP2 下载窗口 48KB 过小；`max_firmware_size` 默认 256KB 可能越界擦除参数区/跳转槽 | `flash_download.h`：`FW_APP_MAX_SIZE=0x14000`(80KB)、`TBOX_ADDR_END=0x08018000`；`flash_download.c`：`max_firmware_size = FW_APP_MAX_SIZE`，并新增 `m+size` 越界检查 |
| 5 | main.c 过期注释“Phase 1：暂时关闭”与代码不符 | 删除注释 |
| 6 | OTA 工程 app1 的 31 ACK 用 raw CAN，与 boot 的 ISOTP 不一致 | app1 改为与 boot 一致（`isotp_send_message`） |
| 7 | OTA 工程 boot/app1/app2 下载上限仍为 48KB（`Bootloader_App.c` 的 `max_firmware_size`/`user_end_addr`、`flash_download.h` 的 `FW_APP_MAX_SIZE`/`TBOX_ADDR_END`） | 全部同步为 80KB |

## 十五、RTT 调试提示

- 四驱时基：`SysTick_Init(1000U)` → `SysTick_Handler` → `SysTick_IncTick`，`SysTick_GetTick()` 返回 uint32_t（ms）；
- 烧录后“不进 debug 看不到 RTT 打印”通常是 **RTT Viewer 连接侧**问题：连接模式/`ForceGo`、RTT 控制块地址（四驱 `_SEGGER_RTT` ≈ `0x1FFFC340`，OTA ≈ `0x1FFF8A94`）、ELF 文件路径是否指向当前工程；
- `MAIN_D("===== main(): app1 =====")` 是 APP 第一条打印，可用来确认程序是否真的在跑。

## 十六、注意事项

- `0x31` 例程控制的 RID 由 TBOX 定义（实测为 `0x01FE`），handler 不校验 RID，只要安全解锁即可触发 Phase 1；
- 四驱 APP 链接地址必须为 `0x1A000`（Keil IROM1 Start），尺寸上限 `0x14000`（80KB）；
- 参数存储 `0x6E000`（扇区 55）是 **APP 自有系统**（`drv_mcu_flash.c`），`0x7C000`（扇区 62）是 **bootloader 跳转槽**，两者都不可被 OTA 下载区（APP2: `0x4C000~0x5FFFF`）覆盖；
- 四驱 `flash_download` 使用 EFM 直写（无 FlashAdvanced 层），OTA 工程 boot 内仍使用 FlashAdvanced。
