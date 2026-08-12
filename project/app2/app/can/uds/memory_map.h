#ifndef MEMORY_MAP_H
#define MEMORY_MAP_H

/*
 * ============================================================
 * 内存 / Flash 分区唯一宏定义源（四驱工程 D:\260706_NL）
 * 新分区：扇区 13~54 平分（168KB / 168KB）
 *   APP1 0x1A000 / APP2 0x44000 / 结束 0x6E000（不碰扇区55）
 * 与 OTA 工程的 memory_map.h 保持同步。
 * ------------------------------------------------------------
 * 【Phase 2 归属说明】
 * 实际 OTA Phase 2（0x34/0x36/0x37 的地址映射、擦除、写入、
 * 跳转槽设置）由 OTA 工程（D:\ota_ddl3.3_v.3.1）的 boot 固件
 * 执行，使用的是 OTA 工程自己 Bootloader_App/memory_map.h 的宏。
 * 因此：
 *   [Phase2无关]   = 本文件里这些宏不参与实际 OTA 擦写，
 *                    修改它们不影响 Phase 2 的行为；
 *   [需与boot一致] = 影响四驱 APP 自身（编译/链接/Phase1/3），
 *                    修改会影响 APP，但不改变 Phase 2 的擦写行为。
 * ============================================================
 */

/* ========== 芯片 / 扇区 ========== */
#define FLASH_SECTOR_SIZE           0x2000UL        /* 8KB/扇区；[Phase2无关] 仅四驱参数存储(drv_mcu_flash)用 */
#define FLASH_TOTAL_SIZE            0x80000UL       /* 512KB；[Phase2无关] 仅参考 */

/* ========== APP 分区（新方案：168KB / 168KB） ========== */
#define APP1_START_ADDR             0x0001A000UL    /* 扇区13，四驱 APP 链接基址；[需与boot一致] 决定 APP 运行位置 */
#define APP2_START_ADDR             0x00044000UL    /* 扇区34，OTA 下载目标；[需与boot一致] Phase2 实际目标由 boot 决定 */
#define APP_MAX_SIZE                0x0002A000UL    /* 168KB；[需与boot一致] 同上 */
#define APP1_END_ADDR               (APP1_START_ADDR + APP_MAX_SIZE)  /* 0x44000；[需与boot一致] */
#define APP2_END_ADDR               (APP2_START_ADDR + APP_MAX_SIZE)  /* 0x6E000；[需与boot一致] */

/* ========== 系统保留 / 跳转 / 参数 ========== */
#define UDS_SHARED_SECTOR_BASE      0x00010000UL    /* 扇区8；[需与boot一致] Phase1/3 与 boot 通信，改前必须与 boot 相同 */
#define UDS_SHARED_MAGIC            0x55445300UL    /* [需与boot一致] 同上 */
#define APP_RUN_SLOT_ADDR           0x0007C000UL    /* 扇区62；[Phase2无关] 四驱 APP 不写跳转槽，实际由 boot 写 */
#define STORAGE_SECTOR_ADDR         0x0006E000UL    /* 扇区55；[Phase2无关] 四驱 APP 参数存储（分区约束：勿被 APP2 覆盖） */
#define STORAGE_MAGIC               0x5AA55AA5UL    /* [Phase2无关] 四驱 APP 参数存储 magic */

/* 向量表偏移 = APP 链接基址（system_hc32f460.c 使用） */
#define VECT_TAB_OFFSET             APP2_START_ADDR /* [需与boot一致] 决定四驱 APP 从哪个地址运行 */

/* ========== 兼容旧名（原 flash_download.h） ========== */
#define FW_APP_START_ADDR           APP2_START_ADDR /* [Phase2无关] 仅四驱 flash_download 编译用，APP 不执行 Phase2 */
#define FW_APP_MAX_SIZE             APP_MAX_SIZE    /* [Phase2无关] 同上 */
#define FW_BOOTLOADER_START_ADDR    0x00000000UL    /* [Phase2无关] 同上 */

/* ========== TBOX 槽位标签（客户协议固定，仅表示烧录到哪个槽） ========== */
#define TBOX_ADDR_APP1              0x88010000UL    /* 标签：烧录到 APP1（0x1A000）；[Phase2无关] 实际映射由 boot 执行 */
#define TBOX_ADDR_APP2              0x48000000UL    /* 标签：烧录到 APP2（0x44000）；[Phase2无关] 实际映射由 boot 执行 */

/* [Phase2无关] 四驱 APP 的 flash_download 不参与实际 OTA，地址映射由 boot 执行 */
#define MAP_TBOX_ADDR_TO_FLASH(addr) \
    (((addr) == TBOX_ADDR_APP1) ? APP1_START_ADDR : \
     ((addr) == TBOX_ADDR_APP2) ? APP2_START_ADDR : (addr))

/* ========== OTA 限制 ========== */
#define FW_MAX_FIRMWARE_SIZE        APP_MAX_SIZE    /* 固件最大 = APP 分区大小 = 168KB；[Phase2无关] 仅四驱 flash_download 编译用 */
#define FW_RAM_BUFFER_SIZE          (8 * 1024)      /* 0x36 块对齐暂存；[Phase2无关] 同上 */

#endif /* MEMORY_MAP_H */