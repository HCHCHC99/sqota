# CAN 模块增补记录

## 日期

2026-07-22

## 背景

为支持后续 UDS 固件刷写（OTA），将 OTA 项目 (`ota_ddl3.3_v.3.1/merge_v.1.0.0/app1`) 中 `can_module` 的增强能力合入四驱分动器控制器。

核心诉求：UDS 多帧传输需要 **TX 完成回调** 和 **发送忙碌查询**，以便上层构建发送队列和流控。

## 变更文件

| 文件 | 变更类型 | 说明 |
|------|----------|------|
| `can_module.h` | 新增 API | 追加 `can_tx_callback_t` 类型、`can_register_tx_callback`、`can_is_tx_busy`、`can_module_irq_handler` |
| `can_module.c` | 增强 | 新增 TX 回调机制、统一中断处理函数、发送忙标志；`can_module_init` 增加 RX 缓存指针保存和清零 |
| `can_hw.c` | 修改 | 中断回调从 `app_can_int_callback` 改为 `can_module_irq_handler`；移除 `app_can.h` 依赖 |
| `app_can.h` | 删除 | 移除 `app_can_int_callback` 函数声明 |
| `app_can.c` | 删除 / 编码修复 | 移除 `app_can_int_callback` 函数实现（~70 行）；文件编码 UTF-16LE → UTF-8 |

## 新增 API 说明

```c
// 注册 TX 完成回调（中断上下文中调用）
void can_register_tx_callback(can_tx_callback_t pfnCallback);

// 查询 TX 硬件是否忙碌
bool can_is_tx_busy(void);

// CAN 中断统一处理入口（替代各层自行编写 ISR）
void can_module_irq_handler(void);
```

## 架构变化

### 改前

```
CAN 中断 → can_hw.c 注册 app_can_int_callback
              └→ app_can.c  自行处理 RX/TX/错误/Bus-Off
```

### 改后

```
CAN 中断 → can_hw.c 注册 can_module_irq_handler
              └→ can_module.c  统一处理 RX/TX/错误/Bus-Off
                       └→ TX 完成时回调 can_register_tx_callback 注册的函数
```

ISR 逻辑从应用层 (`app_can.c`) 收归 HAL 层 (`can_module.c`)，应用层不再关心中断细节。

## 对现有功能的影响

**无。** 车身网络协议所有逻辑（`app_can_receive` / `app_can_transmit` / `app_can_task` / `app_can_init` / 电机状态机 / 消息发布）完全不变。

## 后续待做

- [ ] 在 `app_can_receive` 的 `default` 分支将未知 CAN ID 帧转发至 UDS 处理
- [ ] 从 OTA 项目移植 `Adapter_Can` 层（CAN 帧过滤器 + 发送队列 + 分发机制）
- [ ] 对接 ISO 15765-2 传输层和 UDS 诊断服务
