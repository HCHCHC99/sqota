# 四驱 APP OTA 优化（阶段 2/3）

> 日期: 2026-08-02
> 适用范围: 四驱控制盒 APP（D:\260706_NL），配合 OTA boot（merge_v.1.0.0 的 boot）使用
> 相关文档: [uds移植.md](uds移植.md) ｜ 配套 boot 侧说明见 OTA 工程 `bootOTA优化.md`

---

## 一、背景与目标

### 1.1 阶段 1（既有链路）

正常 OTA 依赖 APP 的 UDS 会话（0x31 例程控制）：

```
APP 收到 10 03 → 27 → 31
  → 写共享区 (phase=1, pending_sid=0x31)
  → 延迟 100ms → NVIC_SystemReset()
  → OTA boot: Boot_StartupSequence 读 phase==1 → Bootloader_UdsMain (Phase 2)
  → 下载到 APP2 → 0x11 → 复位 → APP 补发 51 01 (Phase 3)
```

局限：如果 APP 本身无法运行（崩溃、被坏块标记、异常），0x31 流程无法发起，只能依赖 boot 的“双 APP 故障恢复”。

### 1.2 阶段 2/3 的目标

- **阶段 2**：不依赖 APP 的 0x31 流程也能进入编程模式——boot 上电窗口检测 + APP 运行中检测强制指令；
- **阶段 3**：在 APP 运行中强制切换“下次自动启动的 APP 槽位”（APP1/APP2）。

**本文件只讲四驱 APP 侧的处理**；boot 侧的窗口/决策/回帧见 `bootOTA优化.md`。

---

## 二、指令协议（四驱 APP 只用到指令帧）

| CAN ID | data[0] | 含义 | 四驱 APP 动作 |
|---|---|---|---|
| `0x18FF5858` | `0xFF` | 强制进入 bootloader 编程模式 | 软件复位 → boot 窗口处理 |
| `0x18FF5858` | `0x01` | 强制下次启动 APP1 | 软件复位 → boot 窗口处理 |
| `0x18FF5858` | `0x02` | 强制下次启动 APP2 | 软件复位 → boot 窗口处理 |

> 回帧 `0x18EF5858` 由 **boot 侧**发出（APP 不回复），APP 侧无需关心回帧协议。

---

## 三、四驱 APP 侧的设计

### 3.1 检测点：`uds_rx_entry()`

四驱的 CAN 接收架构与 OTA 工程不同：

```
四驱 RX 路径（唯一）:
  CAN 中断 → can_rx 环形缓存
    → 主循环 app_can_receive()
        ├─ case 车身 ID (0x18FF9DF0 等) → data_process_*()
        └─ default → uds_rx_entry(can_id, data, len)
```

- 四驱的 `CanIf_Poll()` **不做 RX 分发**（移植时移除，所有帧统一走 `app_can_receive`），所以不能像 boot 那样注册 CanIf 过滤器回调；
- `uds_rx_entry` 是所有非车身 ID 的统一入口，0x18FF5858 不属于车身协议，会落到这里——在此加一个独立分支即可，**不污染车身协议、也不经过 ISOTP**。

### 3.2 标志：`g_force_ota_cmd`（uds_ota.c）

`uds_rx_entry` 只负责“记录指令值”，真正的动作在 `UdsOta_Poll()`：

```c
/* uds_rx_entry.c */
if (can_id == BOOT_FORCE_CMD_CAN_ID) {        /* 0x18FF5858 */
    if (len >= 1U) g_force_ota_cmd = data[0];
    return;                                     /* 裸帧，不进 ISOTP */
}

/* uds_ota.c UdsOta_Poll() */
if (g_force_ota_cmd != 0U) {
    uint8_t u8ForceCmd = g_force_ota_cmd;
    g_force_ota_cmd = 0U;
    MAIN_D("Force OTA cmd 0x18FF5858 = 0x%02X, resetting to bootloader...\r\n", ...);
    NVIC_SystemReset();
    while (1) { }
}
```

### 3.3 动作：软件复位，交给 boot

APP 侧**不做任何决策**（不判坏块、不写槽、不回帧），任何非 0 指令一律 `NVIC_SystemReset()`，由 boot 的 50ms 窗口按指令语义处理。这样 APP 与 boot 职责单一、语义一致。

---

## 四、程序调用链路

```
CAN 总线: 0x18FF5858 / 0xFF|0x01|0x02
  │
  ▼
CAN 接收中断 → can_rx 环形缓存
  │
  ▼ (主循环)
app_can_receive()                          app/can/app_can.c
  └─ switch 未匹配 → default:
       ▼
uds_rx_entry(can_id, data, len)            app/can/uds/uds_rx_entry.c
  ├─ can_id == 0x18FF5858 ?
  │     ├─ 是 → g_force_ota_cmd = data[0]; return;   ← 不进 ISOTP
  │     └─ 否 → is_uds_id() → ISOTP → UDS（原有诊断/OTA 流程）
       ▼
UdsOta_Poll()                              app/can/uds/uds_ota.c
  ├─ g_force_ota_cmd != 0
  │     ├─ 打印 "Force OTA cmd 0x18FF5858 = 0xXX"
  │     ├─ NVIC_SystemReset()
  │     └─ while(1)（不返回）
  ▼
OTA boot 启动 → 50ms 窗口 → 按指令语义处理（见 bootOTA优化.md）
```

---

## 五、为什么这样设计调用链路

| 设计点 | 原因 |
|---|---|
| **检测放在 `uds_rx_entry`** | 四驱 `CanIf_Poll` 不分发 RX，CanIf 过滤器回调不会触发；`uds_rx_entry` 是 UDS/OTA 层的唯一入口，且与车身协议天然隔离 |
| **裸帧不进 ISOTP** | 0x18FF5858 是 1 字节指令帧，进 `isotp_receive_frame` 会被当成 ISOTP 单帧（PCI=0xFF/0x01）解析成错误 SID，随后被 UDS CAN ID 过滤丢弃——既检测不到又浪费处理 |
| **只记录标志，动作放 `UdsOta_Poll`** | `uds_rx_entry` 在接收路径（可能高频），只做“置位”；主循环的 `UdsOta_Poll` 是统一轮询点，集中处理动作，逻辑清晰、避免重复代码 |
| **任意非 0 都复位** | 指令语义（进编程模式 / 启 APP1 / 启 APP2）由 boot 窗口裁决；APP 无需感知具体指令，保持简单一致，未来新增指令码也不用改 APP |
| **不写共享区、直接复位** | 方案 A（boot 再检测）：依赖 TBOX 持续发送指令（复位后 boot 50ms 窗口需再收到一帧）。不写 flash 无残留、无磨损 |
| **标志清零后再复位** | 防止复位后（SRAM 保留）同标志残留导致异常 |

---

## 六、与 boot 的分工

| 职责 | 四驱 APP | OTA boot |
|---|---|---|
| 运行中检测指令 | ✅ `uds_rx_entry` → 标志 | — |
| 上电窗口检测指令 | — | ✅ 50ms 窗口 |
| 坏块（WDT≥3）判定 | — | ✅ |
| 写 APP_RUN_SLOT | — | ✅ |
| 回帧 0x18EF5858 | — | ✅ |
| 软复位 | ✅ | ✅（写槽后） |
| 跳转目标 APP | — | ✅ 正常启动序列 |

---

## 七、涉及文件

| 文件 | 改动 |
|---|---|
| `app/can/uds/bootloader_app.h` | 新增 `BOOT_FORCE_CMD_*` 宏（0x18FF5858 / 0xFF / 0x01 / 0x02） |
| `app/can/uds/uds_ota.h` | `extern volatile uint8_t g_force_ota_cmd;` |
| `app/can/uds/uds_ota.c` | 定义标志；`UdsOta_Poll()` 检查并软复位 |
| `app/can/uds/uds_rx_entry.c` | `0x18FF5858` 独立分支置标志 |