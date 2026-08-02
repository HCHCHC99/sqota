# CAN + UDS + OTA 代码分析报告

> 分析日期：2026-07-30
> 分析范围：`project/app/can/`、`project/app/can/uds/`、`project/startup/main.c`

---

## 一、严重 Bug

### 1. `update_motor2_state()` 用了错误的电机索引 — `app_can.c:242`

```c
static void update_motor1_state(void) {
    motor_state_t* pstate = can_get_mag_state(0);  // 电机1 → 索引0 ✓
    ...
}
static void update_motor2_state(void) {
    motor_state_t* pstate = can_get_mag_state(0);  // 电机2 → 应该是索引1 !!!
    ...
}
```

**后果**：电机2 上报的状态实际上是电机1 的数据，两个电机在 CAN 总线上发出相同数据。应改为 `can_get_mag_state(1)`。

### 2. `FlashDownload_Task()` 是空函数 → `pending_response` 永远不会清除 — `flash_download.c:168` / `uds_diagnostic.c`

`FlashDownload_OnTransferData()` 成功写入后设置 `g_ctx.pending_response = true`（第134行），但 `FlashDownload_Task()` 是空函数（第168行），导致这个标志**永远无法清除**。

```c
// flash_download.c:168
void FlashDownload_Task(void) {}  // 空!
```

**后果**：
- 第一个 `TransferData(0x36)` 成功后，`is_pending()` 返回 true
- UDS 层发送 `0x78`（Response Pending）
- TBOX 重试 → `is_pending()` **仍然 true** → 又是 `0x78`
- **OTA 固件下载永远卡住**，无法完成

**修复方向**：要么在 `FlashDownload_Task()` 中清除 `pending_response`，要么不要在 `OnTransferData()` 中无条件设置它（同步写 Flash 不需要延迟响应）。

### 3. `UdsOta_Poll()` 每次主循环被调用两次 — `main.c:127` + `app_can.c:380`

```
main() 主循环:
  Sys_Schedule_Run() → app_can_task() → UdsOta_Poll()  // 第一次
  UdsOta_Poll()                                         // 第二次!
```

```c
// app_can.c:371-381
void app_can_task(void) {
    app_can_receive();
    app_can_transmit();
    can_evt_publish(&s_can_msg);
#if SYS_ENABLE_UDS
    UdsOta_Poll();  // ← 第一次
#endif
}

// main.c:121-128
while (1) {
    Sys_Schedule_Run();  // 内含 app_can_task → UdsOta_Poll()
#if SYS_ENABLE_UDS
    UdsOta_Poll();       // ← 第二次!
#endif
}
```

**后果**：`FlashDownload_Task()` 和 `CanIf_Poll()` 每轮主循环执行两次，浪费 CPU。虽然 1ms 门控部分（`isotp_ms_update/uds_ms_update`）因为有 `s_last_ms_tick` 保护不会重复执行，但无保护的部分被双倍调用。

---

## 二、架构/设计问题

### 4. 两套 UDS 接收通路，一套是死代码

| 通路 | 入口 | 缓冲区 | 状态 |
|------|------|--------|------|
| A | `app_can_receive()` → `default:` → `uds_rx_entry()` | `s_buf[4100]` (uds_rx_entry.c) | **活跃** |
| B | `CanIf_Poll()` → `CanIf_DispatchRx()` → `ISOTP_RxCallback()` | `s_uds_rx_buffer[4100]` (uds_ota.c) | **死代码** |

`CanIf_Poll()` 中注释了 `/* RX dispatch removed */`，所以 `CanIf_DispatchRx()` 永远不会被调用。但 `ISOTP_RegisterRxFilters()` 仍然注册了 4 个 UDS CAN ID 过滤器 —— 这些过滤器注册了但从不会被触发。

**浪费**：两个 4100 字节缓冲区 (~8KB RAM)、16 个过滤器槽位中浪费了 4 个。

### 5. `uds_ota.c` 重复 include `bootloader_app.h`

```c
#include "bootloader_app.h"  // 第16行
#include "bootloader_app.h"  // 第17行 — 重复
```

### 6. 没有 bootloader 却设计了完整的三阶段 OTA

移植文档明确写"四驱控制器目前没有 bootloader，Phase 2 不适用"。但代码中：
- `uds_handle_routine_control(0x31)` 写了共享 Flash 后延迟复位（Phase 1）
- `App_CheckPendingUdsAck()` 只处理 `pending_sid == 0x11`，不处理 `0x31`
- 复位后回到同一个 APP，Phase 2 永远不会执行

**实际效果**：收到 `0x31` 后会复位，但不会执行任何 OTA 操作。如果这是预期行为（仅作为状态机占位），应该加注释说明。

### 7. `can_cfg_init()` 硬编码滤波器，忽略 `cfg->can_filter` 配置 — `can_module.c:83-96`

```c
static void can_cfg_init(CM_CAN_TypeDef *CANx, const can_cfg_t *cfg) {
    stc_can_filter_config_t astcFilter[1] = {
        {0UL, 0x18FFFFFFUL, CAN_ID_STD_EXT}  // 硬编码!
    };
    stcCanInit.pstcFilter = astcFilter;  // 忽略 cfg->can_filter
    // 也不检查 cfg->en_can_filte
}
```

`can_hw.c` 中配置的 `en_can_filte` 和 `can_filter` 字段完全无效。滤波器始终启用且硬编码为匹配 `0x18xxxxxx` 扩展帧。

---

## 三、中等问题

### 8. ECU Reset (0x11) 在 APP 上下文不执行复位 — `uds_diagnostic.c:335-382`

```c
} else {
    MAIN_D("  ECU Reset: APP context, sending normal response\r\n");
    resp[0] = reset_type;
    *resp_len = 1;
    // 没有 NVIC_SystemReset()!
}
```

APP 收到 ECU Reset 请求后，只是发送肯定响应，**实际上并没有复位**。不符合 UDS 规范。

### 9. ISOTP 缓冲区 8KB 但消息长度限制只有 4095 — `isotp_transport.h:81-90`

`ISOTP_BUFFER_SIZE 8192`（8KB）和 `ISOTP_MAX_MESSAGE_LEN 4095`（4KB）不匹配，浪费了约 4KB RAM。

### 10. `can_module_irq_handler` 中嵌套 while 循环可能死循环 — `can_module.c:340-348`

```c
while (CAN_GetRxBufStatus(CANx) != CAN_RX_BUF_EMPTY) {
    while (CAN_GetRxFrame(CM_CAN, &frame) == LL_OK) {
        // ...
    }
}
```

如果硬件状态异常导致 `CAN_GetRxBufStatus` 不返回 EMPTY 且 `CAN_GetRxFrame` 持续返回 `LL_OK`，就会在中断上下文中死循环，系统完全卡死。建议加超时保护或使用 for 循环限制读取次数。

### 11. `UdsOta_Poll()` 中 `s_last_ms_tick` 使用 `uint64_t` — `uds_ota.c:97`

`SysTick_GetTick()` 返回 `uint32_t`，但比较用的是 `uint64_t`。功能上能工作，但类型不匹配容易引起误解。建议统一为 `uint32_t`。

---

## 四、小问题

### 12. `uds_diagnostic.c` 开头格式混乱

`MEM_ZERO_STRUCT` 宏定义夹在 `#include` 之间，且 `#include "bootloader_app.h"` 前面的代码格式异常（第1-7行附近）。

### 13. `isotp_transport.c` 中 `isotp_handle_flow_control_internal` 重复声明

第63行和第67行都声明了同一个 static 函数。

### 14. `common.h` 引入了不必要的大量头文件

`<math.h>`, `<assert.h>`, `<stdlib.h>`, `<stdio.h>` 在整个 UDS 协议栈中大部分都用不到，增加了编译时间和潜在的命名冲突。

---

## 总结优先级

| 优先级 | 问题 | 位置 |
|--------|------|------|
| **P0** | `pending_response` 永远不清理，OTA 下载卡死 | `flash_download.c:168` |
| **P0** | 电机2 状态用了电机1 的索引 | `app_can.c:242` |
| **P1** | `UdsOta_Poll()` 被调用两次 | `main.c:127` + `app_can.c:380` |
| **P1** | 两套 RX 通路 + 两套缓冲区浪费 RAM | `uds_rx_entry.c` / `uds_ota.c` |
| **P1** | ECU Reset 不执行复位 | `uds_diagnostic.c:377-380` |
| **P2** | 无 bootloader 下 Phase 1/2/3 的实际行为不明确 | 全局 |
| **P2** | 硬编码滤波器忽略配置 | `can_module.c:92-93` |
| **P3** | 重复 include、格式问题等 | 多处 |
