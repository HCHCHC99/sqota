#ifndef MEMORY_MAP_H
#define MEMORY_MAP_H

/*
 * ============================================================
 * 内存 / Flash 分区唯一宏定义源（四驱工程 D:\260706_NL）
 * ------------------------------------------------------------
 * 与 OTA 工程（D:\ota_ddl3.3_v.3.1）共享同一套 Flash 布局：
 *   APP1 0x1A000 / APP2 0x4C000 / 80KB / 跳转槽 0x7C000 /
 *   共享区 0x10000 / 参数区 0x6E000（扇区55，四驱参数存储）
 * 修改分区只需改本文件，并同步 Keil Target IROM1 与
 * OTA 工程的 memory_map.h。
 * ============================================================
 */

/* ========== 芯片 / 扇区 ========== */
#define FLASH_SECTOR_SIZE           0x2000UL        /* 8KB/扇区 */
#define FLASH_TOTAL_SIZE            0x80000UL       /* 512KB (HC32F460xE) */

/* ========== APP 分区（当前 80KB / 80KB） ========== */
#define APP1_START_ADDR             0x0001A000UL    /* 扇区13，四驱 APP 链接基址 */
#define APP2_START_ADDR             0x0004C000UL    /* 扇区38，OTA 下载目标 */
#define APP_MAX_SIZE                0x00014000UL    /* 80KB */
#define APP1_END_ADDR               (APP1_START_ADDR + APP_MAX_SIZE)  /* 0x2E000 */
#define APP2_END_ADDR               (APP2_START_ADDR + APP_MAX_SIZE)  /* 0x60000 */

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

/* ========== TBOX 地址窗口（单窗口：APP2，80KB） ========== */
#define TBOX_ADDR_START             0x08004000UL
#define TBOX_ADDR_END               0x08018000UL
#define MAP_TBOX_ADDR_TO_FLASH(addr) \
    (((addr) >= TBOX_ADDR_START && (addr) < TBOX_ADDR_END) ? \
     ((addr) - TBOX_ADDR_START + FW_APP_START_ADDR) : (addr))

/* ========== OTA 限制 ========== */
#define FW_MAX_FIRMWARE_SIZE        APP_MAX_SIZE    /* 固件最大 = APP 分区大小 */
#define FW_RAM_BUFFER_SIZE          (8 * 1024)      /* 0x36 块对齐暂存（仅对齐用） */

#endif /* MEMORY_MAP_H */