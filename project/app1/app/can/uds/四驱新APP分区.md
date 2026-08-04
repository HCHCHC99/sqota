# 四驱新APP分区：memory_map.h 修改说明

> 日期：2026-08-04
> 适用范围：四驱控制盒工程 `D:\260706_NL`
> 目标：扇区 13~54 平分给 APP1/APP2（168KB / 168KB），APP2 首地址从 `0x4C000` 改为 `0x44000`

---

## 一、新布局

扇区 13~54 共 42 个扇区（8KB/扇区），平分 = 每边 21 个扇区 = **168KB（0x2A000）**

| 槽位 | 扇区 | 地址范围 | 大小 |
|---|---|---|---|
| **APP1**（四驱 APP 运行槽） | 13–33 | `0x1A000 ~ 0x43FFF` | 168KB |
| **APP2**（OTA 下载目标） | 34–54 | `0x44000 ~ 0x6DFFF` | 168KB |

- APP2 结束 `0x6E000` 正好是 55 号扇区起点，**不占用参数存储（0x6E000，扇区55）** ✅
- APP1 起始地址不变（`0x1A000`），所以 `VECT_TAB_OFFSET` 不用改

---

## 二、memory_map.h 需要改的宏（`app/can/uds/memory_map.h`）

| 宏 | 旧值 | 新值 |
|---|---|---|
| `APP2_START_ADDR` | `0x0004C000UL` | `0x00044000UL` |
| `APP_MAX_SIZE` | `0x00014000UL`（80KB） | `0x0002A000UL`（168KB） |
| `APP1_END_ADDR` | （自动） | `APP1_START_ADDR + APP_MAX_SIZE` = `0x44000` |
| `APP2_END_ADDR` | （自动） | `APP2_START_ADDR + APP_MAX_SIZE` = `0x6E000` |
| `TBOX_ADDR_START`（四驱单窗口=APP2窗口） | `0x08004000UL` | `0x08042000UL` |
| `TBOX_ADDR_END` | `0x08018000UL` | `0x0806C000UL` |

不用改：`APP1_START_ADDR`（0x1A000）、`VECT_TAB_OFFSET`（=APP1_START_ADDR 自动跟随）、`STORAGE_SECTOR_ADDR`（0x6E000）、`FW_APP_START_ADDR/FW_APP_MAX_SIZE`（别名自动跟随）、`FW_RAM_BUFFER_SIZE`（8KB，够用）。

---

## 三、完整新版 memory_map.h（可直接覆盖）

```c
#ifndef MEMORY_MAP_H
#define MEMORY_MAP_H

/*
 * ============================================================
 * 内存 / Flash 分区唯一宏定义源（四驱工程 D:\260706_NL）
 * 新分区：扇区 13~54 平分（168KB / 168KB）
 *   APP1 0x1A000 / APP2 0x44000 / 结束 0x6E000（不碰扇区55）
 * 与 OTA 工程的 memory_map.h 保持同步。
 * ============================================================
 */

/* ========== 芯片 / 扇区 ========== */
#define FLASH_SECTOR_SIZE           0x2000UL        /* 8KB/扇区 */
#define FLASH_TOTAL_SIZE            0x80000UL       /* 512KB (HC32F460xE) */

/* ========== APP 分区（新方案：168KB / 168KB） ========== */
#define APP1_START_ADDR             0x0001A000UL    /* 扇区13，四驱 APP 链接基址 */
#define APP2_START_ADDR             0x00044000UL    /* 扇区34，OTA 下载目标 */
#define APP_MAX_SIZE                0x0002A000UL    /* 168KB */
#define APP1_END_ADDR               (APP1_START_ADDR + APP_MAX_SIZE)  /* 0x44000 */
#define APP2_END_ADDR               (APP2_START_ADDR + APP_MAX_SIZE)  /* 0x6E000 */

/* ========== 系统保留 / 跳转 / 参数 ========== */
#define UDS_SHARED_SECTOR_BASE      0x00010000UL    /* 扇区8 */
#define UDS_SHARED_MAGIC            0x55445300UL
#define APP_RUN_SLOT_ADDR           0x0007C000UL    /* 扇区62，boot 跳转槽 */
#define STORAGE_SECTOR_ADDR         0x0006E000UL    /* 扇区55，四驱参数存储 */
#define STORAGE_MAGIC               0x5AA55AA5UL

/* 向量表偏移 = APP1 链接基址（system_hc32f460.c 使用） */
#define VECT_TAB_OFFSET             APP1_START_ADDR

/* ========== 兼容旧名（原 flash_download.h） ========== */
#define FW_APP_START_ADDR           APP2_START_ADDR
#define FW_APP_MAX_SIZE             APP_MAX_SIZE
#define FW_BOOTLOADER_START_ADDR    0x00000000UL

/* ========== TBOX 地址窗口（APP2 窗口，168KB，与 OTA 工程一致） ========== */
#define TBOX_ADDR_START             0x08042000UL
#define TBOX_ADDR_END               0x0806C000UL
#define MAP_TBOX_ADDR_TO_FLASH(addr) \
    (((addr) >= TBOX_ADDR_START && (addr) < TBOX_ADDR_END) ? \
     ((addr) - TBOX_ADDR_START + FW_APP_START_ADDR) : (addr))

/* ========== OTA 限制 ========== */
#define FW_MAX_FIRMWARE_SIZE        APP_MAX_SIZE    /* 固件最大 = APP 分区大小 = 168KB */
#define FW_RAM_BUFFER_SIZE          (8 * 1024)      /* 0x36 块对齐暂存（仅对齐用） */

#endif /* MEMORY_MAP_H */
```

---

## 四、其它必须同步修改的地方

| 位置 | 修改 |
|---|---|
| **Keil Target IROM1**（`template - 副本.uvprojx` 或 GUI） | Start `0x1A000`（不变），Size `0x14000` → **`0x2A000`** |
| **OTA 工程 boot** | 四驱不编译 boot，但 OTA 下载/跳转由 OTA 工程的 boot 负责，**必须按 `OTA工程新APP分区.md` 同步修改**（否则 0x34 窗口/跳转槽不匹配） |
| **TBOX 侧** | 0x34 地址窗口：烧 APP2 发 `0x08042000`（不再是 `0x08004000`）；烧 APP1 发 `0x08018000` |

---

## 五、验证清单

- [ ] 四驱工程 Keil 编译通过，无 L6218/溢出错误
- [ ] OTA 工程 boot 已按新分区同步（`OTA工程新APP分区.md`）
- [ ] TBOX 0x34 发 `0x08042000`（168KB）→ 映射 `0x44000`，返回 `74 00 40 00`
- [ ] 0x36 传满 168KB（672 块，序号回绕已修复）→ 0x37 CRC 通过
- [ ] 擦除范围不越过 `0x6E000`（不碰扇区55 参数存储）
- [ ] 复位后按实际烧录槽位跳转（烧 APP2 跳 APP2 / 烧 APP1 跳 APP1）