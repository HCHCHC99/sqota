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
| `app/can/app_can.c` | `app_can_init` 加 `CanIf_Init(&can_handle)` + `UdsOta_Init()`；`default:` 加 `uds_rx_entry()`；`app_can_task` 加 `UdsOta_Poll()` |
| `app/can/app_can.h` | 加 `extern can_handle_t can_handle`（供 UDS 适配层引用） |
| `startup/main.c` | 加 `SCB->VTOR = APP1_START_ADDR`、`UdsOta_App_CheckPendingAck()`、`while(1)` 中 `UdsOta_Poll()` |
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
main():
  SCB->VTOR = APP1_START_ADDR            ← OTA 对齐
  LL_PERIPH_WE(...)
  BSP_CLK_Init()                         ← 四驱硬件时钟
  SysTick_Init(1000U)                    ← 1ms 时基
  __enable_irq()
  System_Init()                          ← 内含 app_can_init → CanIf_Init + UdsOta_Init
  UdsOta_App_CheckPendingAck()           ← Phase 3: 补发 51 01 ACK（读 0x10000 扇区）
  LL_PERIPH_WP(...)
  while(1):
    Sys_Schedule_Run()                   ← 内含 app_can_task → UdsOta_Poll
    UdsOta_Poll()                        ← Phase 1: 延迟复位倒计时（直接调用）
```

---

## 七、UDS OTA 三阶段（Phase 1/2/3）

| 阶段 | 运行固件 | 触发 | 动作 |
|------|---------|------|------|
| **Phase 1** | APP | 收到 0x31（RoutineControl） | 写 `0x10000` 共享 Flash → 设置 `g_delayed_reset_ms=100` → `UdsOta_Poll()` 倒计时 → `NVIC_SystemReset()` |
| **Phase 2** | Bootloader | 读到 `phase==1` | 补发 31 ACK → UDS 下载（`10 02` → `27` → `34` → `36×N` → `37`）→ 写 `phase=2, result=1` → 等 `0x11` → 复位 |
| **Phase 3** | APP（重新启动） | 读到 `pending_sid==0x11` | 发送 `51 01` ACK → 擦除 `0x10000` → OTA 完成 |

> **注意**：四驱控制器目前没有 bootloader，Phase 2 不适用。Phase 1 的复位后如果进入 bootloader（由 bootloader 的 `Boot_StartupSequence` 判断），需要 bootloader 存在才能走完完整流程。

---

## 八、Flash 布局

| 地址 | 大小 | 用途 |
|------|------|------|
| `0x00000000` | 128KB | Bootloader（预留，当前未实现） |
| `0x00010000` | 8KB | **UDS 共享状态**（`stc_uds_shared_t`，56字节） |
| `0x0001A000` | ~196KB | APP1（当前固件） |
| `0x0004C000` | ~196KB | APP2（OTA 下载目标） |
| `0x0007C000` | 8KB | 参数存储（`drv_mcu_flash.c` 使用） |

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
