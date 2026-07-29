# 四驱控制盒偏移到 0x1A000 后 SysTick 不更新 — 根因分析

## 仓库

- **本仓库** (sqota): `D:\260706_NL` — 四驱控制盒应用代码
- **Bootloader 仓库** (OTA_merge): `D:\ota_ddl3.3_v.3.1` — bootloader + app1

## 现象

四驱控制盒程序偏移到 0x1A000，由 bootloader 跳转启动后，SysTick_Handler 无法正常触发，`SysTick_GetTick()` 永远返回 1（只触发了一次就不再累加）。

注释掉 `System_Init()` 后 SysTick 恢复正常。进一步在 `sys_init.c` 中逐模块二分排查，最终定位到 `bsp_storeage_init()`（storage 模块）。

## 根因链条

### 第一层：Flash 地址冲突

Bootloader (`OTA_merge` 仓库 `Bootloader_App.h`) 和 storage 模块 (`drv_mcu_flash.c`) 共用同一 flash 扇区：

| 来源 | 文件 | 宏 | 地址 |
|------|------|-----|------|
| Bootloader | `Bootloader_App.h` | `APP_RUN_SLOT_ADDR` | `0x0007C000` |
| storage 模块 | `project/drivers/drv/drv_mcu_flash.c` | `STORAGE_SECTOR_ADDR` | `0x0007C000` |

两个模块在此地址写入不同的 magic 值：

| 写入者 | 写入值 | 含义 |
|--------|--------|------|
| Bootloader | `0x5A5A5A5A` | `SLOT_A_MAGIC` |
| Bootloader | `0xA5A5A5A5` | `SLOT_B_MAGIC` |
| storage 模块 | `0x5AA55AA5` | `STORAGE_MAGIC` |

### 第二层：Magic 不匹配触发擦除

每次启动流程：

1. **Bootloader 运行** → 写 `0x7C000`（写入 `SLOT_A_MAGIC` 或 `SLOT_B_MAGIC`）
2. **Bootloader 跳转到 APP (0x1A000)**
3. **APP 的 `System_Init()`** → `bsp_storeage_init()` → `storage_init()` → `flash_init()` → `flash_find_last()`

`flash_find_last()` 从 `0x7C000` 开始扫描，每次读 4 字节比较 `STORAGE_MAGIC`：

```c
// project/drivers/drv/drv_mcu_flash.c
static uint32_t flash_find_last(flash_t *self, uint32_t blk_size) {
    uint32_t addr = self->priv.sector_addr;  // 0x7C000
    uint32_t last = 0;
    while (addr < self->priv.sector_addr + self->priv.sector_size) {
        uint32_t magic;
        self->ops->read(self, addr, (uint8_t*)&magic, 4);
        if (magic != self->priv.magic) break;  // 读到的 SLOT_A_MAGIC ≠ STORAGE_MAGIC → 立即退出
        last = addr;
        addr += blk_size;
    }
    return last;  // 返回 0
}
```

第一个 4 字节就是 bootloader 的 `SLOT_A_MAGIC (0x5A5A5A5A)`，不等于 `STORAGE_MAGIC (0x5AA55AA5)`，循环立即退出，返回 0。

`flash_init()` 收到 0 → 认为扇区未初始化 → 调用 `flash_erase()` 擦除整个扇区：

```c
static int32_t flash_init(flash_t *self, uint32_t blk_size) {
    self->priv.last_valid_addr = self->priv_ops->find_last_valid(self, blk_size);
    if (self->priv.last_valid_addr == 0) {
        self->priv_ops->erase(self);  // ← 擦除 0x7C000 扇区 (8KB)
        // ...
    }
}
```

### 第三层：宏用反导致中断永久关闭

`project/startup/main.h` 中的宏命名与实际行为相反：

```c
// main.h 第 15-18 行
// 实际行为：保存 PRIMASK，然后 __disable_irq()（进入临界区）
#define INTERRUPT_ENABLE(_irq_sta)   do{ _irq_sta = __get_PRIMASK(); __disable_irq(); }while(0)

// 实际行为：__set_PRIMASK() 恢复之前的值（退出临界区）
#define INTERRUPT_DISABLE(_irq_sta)  do{ __set_PRIMASK(_irq_sta); }while(0)
```

`drv_mcu_flash.c` 中的调用也是反的：

```c
// flash_unlock: 应该"关中断进入临界区"，但实际调了 INTERRUPT_DISABLE
static void flash_unlock(flash_t *self) {
    INTERRUPT_DISABLE(self->priv.irq_level);  // __set_PRIMASK(0) → 等于没关中断！
    LL_PERIPH_WE(LL_PERIPH_EFM);
    // ... flash 擦除/写入在中断完全开启的状态下进行 ...
}

// flash_lock: 应该"恢复中断退出临界区"，但实际调了 INTERRUPT_ENABLE
static void flash_lock(flash_t *self) {
    // ... flash 操作完成 ...
    INTERRUPT_ENABLE(self->priv.irq_level);   // __disable_irq() → 永久关闭中断！
}
```

**执行路径追踪**：`flash_erase()` → `flash_unlock()` → `flash_lock()`：

1. `flash_unlock()`: `self->priv.irq_level` 初始值为 0 → `INTERRUPT_DISABLE(0)` → `__set_PRIMASK(0)` → PRIMASK 仍为 0，**中断开着**
2. Flash 擦除在中断开启状态下执行
3. `flash_lock()`: `INTERRUPT_ENABLE(self->priv.irq_level)` → 保存 PRIMASK(0) 到 `irq_level`，然后 `__disable_irq()` → **PRIMASK = 1，中断永久关闭**

### 结果

`PRIMASK = 1` 后，SysTick（异常 #15）被永久屏蔽。SysTick 计数器 `m_u32TickCount` 在擦除前只来得及触发一次（由 `SysTick_Init` 启动后到 `storage_init` 之间），之后不再累加，表现就是 `SysTick_GetTick()` 永远返回 1。

> **注意**：bootloader 的 `APP_RUN_SLOT_ADDR` 扇区也会被 storage 模块擦除，bootloader 的 slot 标志丢失，可能导致后续启动异常。

### 为什么非偏移版本没问题？

非偏移版本（从 0x0 启动，无独立 bootloader）：
- 首次运行：`0x7C000` 扇区为初始状态（全 `0xFF`），`flash_find_last()` 返回 0 → 触发擦除 → 中断被关。**但第一次运行时没有 bootloader 提前写入，所以至少这第一次 SysTick 也会被关。** 然后 storage 写入了 `STORAGE_MAGIC`。
- 再次运行：`flash_find_last()` 找到 `STORAGE_MAGIC`，返回有效地址，不进擦除分支，不会触发 bug。
- 只要没有 bootloader 反复覆盖 `0x7C000`，storage 就能正常工作。

偏移版本（0x1A000 + bootloader）：
- **每次**启动 bootloader 都会覆盖 `0x7C000`（写入 slot magic）
- **每次** APP 启动 storage 都找不到 `STORAGE_MAGIC` → 擦除 → 中断被关
- **每次**都死

## 修复方案

### 方案 A（必须修）：修正 `drv_mcu_flash.c` 的宏调用顺序

文件：`project/drivers/drv/drv_mcu_flash.c`

```c
// flash_unlock: 进入临界区，关中断保护 flash 操作
static void flash_unlock(flash_t *self) {
    INTERRUPT_ENABLE(self->priv.irq_level);   // 改：保存 PRIMASK + __disable_irq
    LL_PERIPH_WE(LL_PERIPH_EFM);
    while (SET != EFM_GetStatus(EFM_FLAG_RDY));
    EFM_FWMC_Cmd(ENABLE);
}

// flash_lock: 退出临界区，恢复原中断状态
static void flash_lock(flash_t *self) {
    EFM_FWMC_Cmd(DISABLE);
    LL_PERIPH_WP(LL_PERIPH_EFM);
    INTERRUPT_DISABLE(self->priv.irq_level);  // 改：恢复 PRIMASK
}
```

### 方案 B（也应该修）：解决地址冲突

将 `STORAGE_SECTOR_ADDR` 改为不与 bootloader 冲突的扇区：

```c
// project/drivers/drv/drv_mcu_flash.c
#define STORAGE_SECTOR_ADDR      0x0007E000UL  // 扇区 63（最后一个 8KB 扇区）
```

HC32F460 Flash 地址规划（512KB, 0x00000~0x7FFFF）：

| 区域 | 地址范围 | 使用者 |
|------|----------|--------|
| Bootloader | `0x00000` ~ `0x19FFF` | bootloader 固件 |
| APP1 | `0x1A000` ~ `0x4BFFF` | 四驱控制盒 / app1 |
| APP2 | `0x4C000` ~ `0x7BFFF` | app2 (OTA 目标) |
| APP_RUN_SLOT | `0x7C000` | bootloader slot 标志 |
| **STORAGE (建议)** | **`0x7E000`** | storage 配置数据（空闲） |

### 建议

两个方案都要实施。方案 A 修复中断被意外关闭的 bug（防止任何类似的 flash 操作导致中断丢失），方案 B 解决地址冲突（避免 storage 模块和 bootloader 互相破坏数据）。

## 排查过程

此问题在 `OTA_merge` 仓库的同份代码上排查，具体步骤：

1. 注释 `System_Init()` → SysTick 正常 → 确认问题在模块注册中
2. 在 `sys_config.h` 中关宏 → 链接错误（其他文件引用模块函数）
3. 改为在 `sys_init.c` 中用 `#if 0` 逐模块禁用，避免链接错误
4. 单禁 soft_timer → 不恢复
5. 单禁 CAN → 不恢复
6. 单禁 power → 不恢复
7. 全禁 16 个模块 → SysTick 恢复
8. 逐批恢复，缩到 5 个：storage、PWM、pos_module、Ctrl_Pos、Ctrl_Speed
9. 缩到 2 个：storage、PWM
10. 单测 PWM → 正常；单测 storage → 异常
11. 阅读 `bsp_storeage_init()` → `storage_init()` → `flash_init()` → `flash_erase()` → `flash_unlock/lock`
12. 发现 `INTERRUPT_DISABLE/ENABLE` 宏命名与实际行为相反，且 `flash_unlock/lock` 中调用顺序也反了
13. 发现 `STORAGE_SECTOR_ADDR (0x7C000)` 与 bootloader 的 `APP_RUN_SLOT_ADDR` 地址冲突
