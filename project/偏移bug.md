# 四驱控制盒偏移到 0x1A000 后 SysTick 不工作 — 根因分析

## 现象

四驱控制盒程序偏移到 0x1A000 烧录后，SysTick_Handler 无法正常触发，`SysTick_GetTick()` 永远返回 1（只触发了一次）。

注释掉 `System_Init()` 后恢复正常，进一步二分排查定位到 `bsp_storeage_init()`（storage 模块）。

## 根因链条

### 1. Flash 地址冲突

Bootloader 和 storage 模块共用 flash 扇区 `0x7C000`：

| 模块 | 定义 | 地址 | 写入内容 |
|------|------|------|----------|
| Bootloader (`Bootloader_App.h`) | `APP_RUN_SLOT_ADDR` | `0x7C000` | `SLOT_A_MAGIC = 0x5A5A5A5A` |
| storage (`drv_mcu_flash.c`) | `STORAGE_SECTOR_ADDR` | `0x7C000` | `STORAGE_MAGIC = 0x5AA55AA5` |

### 2. Magic 不匹配触发擦除

`flash_init()` → `flash_find_last()` 扫描 `0x7C000` 扇区，寻找 `STORAGE_MAGIC`。由于 bootloader 写入了 `SLOT_A_MAGIC`，找不到匹配的 magic，返回 0。

`flash_init()` 认为扇区未初始化，调用 `flash_erase()` 擦除整个 `0x7C000` 扇区（8KB）。

### 3. 宏用反导致中断永久关闭

`main.h` 中的宏（命名本身就是反的）：

```c
// 实际行为：保存 PRIMASK，然后关中断（进入临界区）
#define INTERRUPT_ENABLE(_irq_sta)   do{ _irq_sta = __get_PRIMASK(); __disable_irq(); }while(0)

// 实际行为：恢复 PRIMASK（退出临界区）
#define INTERRUPT_DISABLE(_irq_sta)  do{ __set_PRIMASK(_irq_sta); }while(0)
```

`drv_mcu_flash.c` 中调用也是反的：

```c
static void flash_unlock(flash_t *self) {
    INTERRUPT_DISABLE(self->priv.irq_level);  // 恢复 PRIMASK（初始值=0，等于没关中断！）
    // ... flash 擦除/写入在中断开启状态下进行 ...
}

static void flash_lock(flash_t *self) {
    // ... flash 操作完成后 ...
    INTERRUPT_ENABLE(self->priv.irq_level);   // 保存 PRIMASK + __disable_irq()
                                              // → 永久关闭中断！
}
```

### 4. 结果

`__disable_irq()` 被调用后 PRIMASK=1，SysTick（以及所有其他中断）被永久屏蔽。

因为 `flash_init()` 在第一次 `bsp_storeage_init()` → `storage_init()` → `flash_init()` 时就擦除了扇区，SysTick 在擦除完成后就被关掉了。SysTick 只来得及触发一次（tick=1），之后再也无法触发。

### 5. 为什么非偏移版本没问题？

非偏移版本（从 0x0 启动）没有独立 bootloader。第一次运行时 storage 模块正常初始化了 `0x7C000` 扇区（写入 `STORAGE_MAGIC`），后续启动时 `flash_find_last()` 能找到 magic，不进擦除分支，不会触发 `flash_lock/unlock` 的 bug。

偏移版本每次启动流程：
1. Bootloader 运行 → 写 `0x7C000`（SLOT_A_MAGIC）
2. 跳转到 app 0x1A000
3. app 的 `System_Init()` → `bsp_storeage_init()` → `flash_find_last()` 找不到 `STORAGE_MAGIC` → 擦除 → 中断永久关闭

## 修复方案

### 方案 A（立即修复）：修正 `drv_mcu_flash.c` 中的宏调用

```c
// flash_unlock: 进入临界区 → 关中断
static void flash_unlock(flash_t *self) {
    INTERRUPT_ENABLE(self->priv.irq_level);   // 改：保存PRIMASK + 关中断
    LL_PERIPH_WE(LL_PERIPH_EFM);
    while (SET != EFM_GetStatus(EFM_FLAG_RDY));
    EFM_FWMC_Cmd(ENABLE);
}

// flash_lock: 退出临界区 → 恢复中断
static void flash_lock(flash_t *self) {
    EFM_FWMC_Cmd(DISABLE);
    LL_PERIPH_WP(LL_PERIPH_EFM);
    INTERRUPT_DISABLE(self->priv.irq_level);  // 改：恢复PRIMASK
}
```

### 方案 B（根本修复）：解决地址冲突

将 `drv_mcu_flash.c` 中的 `STORAGE_SECTOR_ADDR` 改为与 bootloader 不冲突的空闲扇区：

```c
#define STORAGE_SECTOR_ADDR      0x0007E000UL  // 或其他空闲扇区
```

确保新地址不与以下地址冲突：
- Bootloader: `0x00000` ~ `0x19FFF`
- APP1: `0x1A000` ~ `0x4BFFF`
- APP2: `0x4C000` ~ `0x7BFFF`
- APP_RUN_SLOT: `0x7C000`
- APP1_STATE_SECTOR: `0x16000`
- APP2_STATE_SECTOR: `0x18000`
- UDS_SHARED_SECTOR: `0x10000`

### 建议

两个方案都要实施：
1. 先修方案 A 防止中断被意外关闭
2. 再修方案 B 解决地址冲突，避免 storage 模块破坏 bootloader 的 slot 标志

## 排查过程

1. 注释 `System_Init()` → SysTick 正常 → 确认问题在模块注册中
2. 单禁 soft_timer → 不恢复
3. 单禁 CAN → 不恢复
4. 单禁 power → 不恢复
5. 全禁 16 个模块 → 恢复 → 确认问题在模块中
6. 逐个恢复，定位到 storage 模块
7. 阅读 `bsp_storeage_init()` → `flash_init()` → `flash_erase()` → `flash_unlock/lock`
8. 发现 `INTERRUPT_DISABLE/ENABLE` 宏命名反了且调用也反了
9. 发现 `STORAGE_SECTOR_ADDR = 0x7C000` 与 `APP_RUN_SLOT_ADDR` 冲突
