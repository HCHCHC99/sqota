# CAN 模块三层拆分说明

## 原始问题

`can_module.h`（487 行）把三个不同关注点的代码全塞在一个文件里：

```
can_module.h (原始)
├── 🔧 硬件抽象 (120行) — GPIO/波特率/滤波器/中断/收发API
├── 📡 协议定义 (200行) — 各 CAN ID 的位域结构体/收发联合体
└── 🏭 业务枚举 (80行)  — 电机指令/档位/标定/故障状态机
```

导致的问题：
- **复用困难**：bootloader 只需要收发 CAN 帧，却被迫引入电机状态机、档位等业务定义
- **编译污染**：HAL 层 `#include <string.h>`、`#include <stdio.h>` 污染了所有使用者
- **修改风险**：改一个 CAN ID 结构体可能影响完全不相关的模块
- **测试困难**：无法单独测试硬件层，必须拉进整个业务层

## 拆分后的三层架构

```
┌─────────────────────────────────────────────────────┐
│  app_can.h / app_can.c                               │
│  应用层: 电机状态机、消息收发调度、业务发布            │
│  职责: 接收 CAN 报文 → 解析 → 发布                   │
│        采集电机状态 → 组帧 → 发送                    │
├─────────────────────────────────────────────────────┤
│  can_protocol.h                                      │
│  协议层: CAN ID 数据结构、位域定义、枚举              │
│  职责: 定义每个 CAN ID 的字节布局                     │
│        rx_data_t / tx_data_t 联合体                  │
│        速度提取宏                                    │
├─────────────────────────────────────────────────────┤
│  can_module.h / can_module.c / can_hw.c              │
│  硬件抽象层: GPIO/波特率/滤波器/中断/收发原语         │
│  职责: 配置 CAN 外设、收发帧、环形缓冲                │
│        不关心 CAN ID 含义，只传递原始字节              │
└─────────────────────────────────────────────────────┘
```

### 各文件职责

| 文件 | 层 | 内容 | 依赖 |
|------|-----|------|------|
| `can_module.h` | HAL | `can_cfg_t`, `can_handle_t`, `can_gpio_t`, `can_frame_t`, 公共 API 声明 | `hc32_ll.h`, `<stdint.h>` |
| `can_module.c` | HAL | GPIO/波特率/CAN外设/中断 初始化, `can_transmit_std/ext`, `can_rx_cache_put`, `can_read`, `can_receive_poll` | `can_module.h` |
| `can_hw.c` | HAL | `CAN_HW` 配置常量实例 | `can_module.h`, `app_can.h` |
| `can_protocol.h` | 协议 | `can_cmd_t`, `gear_status_t`, `run_stat_t`, `fault_stat_t` 等枚举; `can_18FF9DF0_t` ~ `can_18FF0104_t` 各 ID 结构体; `can_rx_data_t`/`can_tx_data_t` 联合体; `GET_SPEED` 宏 | `can_module.h` |
| `app_can.h` | 应用 | `motor_state_t`, `can_msg_t`, `app_can_init/task/int_callback` 等 API | `can_protocol.h` |
| `app_can.c` | 应用 | 接收解析 `app_can_receive`, 定时发送 `app_can_transmit`, 中断回调 `app_can_int_callback`, 电机状态更新 | `app_can.h` + 系统头 |

### 依赖链

```
can_module.h          ← 无业务依赖，只依赖 hc32_ll.h
    ↑
can_protocol.h        ← 引入协议枚举和 CAN ID 结构
    ↑
app_can.h             ← 引入电机状态和消息类型
    ↑
app_can.c             ← 完整应用逻辑
```

## 如何在 bootloader 中单独使用 HAL 层

bootloader 只需要收发 CAN 帧，不关心帧内容。只需引用 `can_module.h`：

### 1. 文件清单

把以下文件复制到 bootloader 工程：

```
can_module.h    — HAL 类型定义 + API 声明
can_module.c    — HAL 实现
can_hw.c        — 硬件配置（按需修改 CAN_TX/RX 引脚）
```

### 2. 配置硬件

编辑 `can_hw.c`（或直接在代码中定义），填充 `can_cfg_t`：

```c
#include "can_module.h"

const can_cfg_t CAN_HW = {
    .can_ins     = CAN1,
    .CANx        = CM_CAN,
    .can_bdr     = CAN_BDR_250K,
    .work_mode   = CAN_WORK_MD_NORMAL,

    .en_can_rx   = CAN_FUNC_ENABLE,
    .gpio_rx     = { .port = GPIO_PORT_B, .pin = GPIO_PIN_14, .func = GPIO_FUNC_51 },
    .can_rx_cfg  = {
        .rx_warn_lmt  = 8U,
        .err_warn_lmt = 10U,
        .rx_all_frame = CAN_RX_ALL_FRAME_DISABLE,
        .rx_ovf_mode  = CAN_RX_OVF_SAVE_NEW,
        .self_ack     = CAN_SELF_ACK_DISABLE,
    },

    .en_can_tx   = CAN_FUNC_ENABLE,
    .gpio_tx     = { .port = GPIO_PORT_B, .pin = GPIO_PIN_15, .func = GPIO_FUNC_50 },
    .can_tx_cfg  = {
        .en_ptb_single_shot = CAN_PTB_SINGLESHOT_TX_DISABLE,
        .en_stb_single_shot = CAN_STB_SINGLESHOT_TX_DISABLE,
        .en_stb_prio_md     = CAN_STB_PRIO_MD_DISABLE,
    },

    /* 中断配置 */
    .can_int_type = (CAN_INT_RX | CAN_INT_PTB_TX | CAN_INT_ERR_INT),
    .can_int      = {
        .can_int_irqn     = INT122_IRQn,
        .can_int_pri      = DDL_IRQ_PRIO_07,
        .can_int_callback = my_can_isr_callback,
    },

    /* 滤波器 */
    .en_can_filte = FILTER_DISABLE,
};
```

### 3. 初始化和使用

```c
#include "can_module.h"

extern const can_cfg_t CAN_HW;   /* 或直接在 .c 中定义 */
static can_handle_t g_can;

// ========== 初始化 ==========
void my_can_init(void)
{
    LL_PERIPH_WE(LL_PERIPH_GPIO | LL_PERIPH_FCG);
    can_module_init(&g_can, &CAN_HW);
    LL_PERIPH_WP(LL_PERIPH_GPIO | LL_PERIPH_FCG);
}

// ========== 发送 ==========
void my_can_send(uint32_t id, uint8_t *data, uint8_t len)
{
    if (id <= 0x7FF) {
        can_transmit_std(id, data, len);
    } else {
        can_transmit_ext(id, data, len);
    }
}

// ========== 接收 (轮询) ==========
void my_can_poll(void)
{
    stc_can_rx_frame_t frame;
    if (can_read(&g_can.can_rx, &frame) == CAN_RET_OK) {
        // frame.u32ID  — CAN ID
        // frame.au8Data — 8 字节数据
        // frame.DLC    — 数据长度
        handle_frame(frame.u32ID, frame.au8Data, frame.DLC);
    }
}

// ========== 中断回调 (由 HAL 在 ISR 中调用) ==========
void my_can_isr_callback(void)
{
    can_receive_poll(&g_can.can_rx);  // 硬件 FIFO → 软件环形缓冲
}
```

### 4. 关键对比

| | 原始 `can_module.h` | 拆分后 `can_module.h` |
|---|---|---|
| 行数 | 487 | ~230 |
| 依赖 | `hc32_ll.h` + `<string.h>` + `<stdio.h>` | `hc32_ll.h` + `<stdint.h>` |
| 引入的业务类型 | `can_cmd_t`, `gear_status_t`, `run_stat_t`, `fault_stat_t`, 9 个 CAN ID 结构体... | 无 |
| bootloader 可用性 | ❌ 引入大量无用定义 | ✅ 纯 HAL，只收发原始字节 |

## 完整应用的使用方式

三层一起使用时，只需包含顶层头文件：

```c
#include "app_can.h"  // 自动拉入 can_protocol.h → can_module.h

void main(void)
{
    app_can_init();   // 内部调用 can_module_init(&can_handle, &CAN_HW)
    while (1) {
        app_can_task();  // 内部调用 app_can_receive + app_can_transmit
    }
}
```

## 修改日志

| 日期 | 说明 |
|------|------|
| 2026-07-09 | 初始拆分：从 `can_module.h` 拆出 `can_protocol.h` + 精简约 `can_module.h`；`app_can.h` 保留业务层 |
