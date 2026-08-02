#ifndef __BOOTLOADER_APP_H__
#define __BOOTLOADER_APP_H__

/* ====== UDS Debug Printing (J-Link RTT Viewer) ====== */
#define UDS_DEBUG       1   // uds_diagnostic: UDS_D/I/W/E
#define ISO_DEBUG       1   // isotp_transport: ISOTP_D/I/W/E
#define OTA_DEBUG       1   // isotp_transport: OTA_D/I/W/E (CAN ID frame-level prints)
/* CANIF_D, FW_D, DL_D are always-on, no #ifdef gate */

#include "hc32_ll.h"
#include <stdint.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

#define APP1_START_ADDR           0x0001A000UL
#define APP2_START_ADDR           0x0004C000UL
#define APP_RUN_SLOT_ADDR         0x0007C000UL
#define UDS_SHARED_SECTOR_BASE    0x00010000UL
#define UDS_SHARED_MAGIC          0x55445300UL
#define UDS_TARGET_FLASH_ADDR     APP2_START_ADDR
#define UDS_POST_FLASH_BOOT_ADDR  APP1_START_ADDR
#define DELAYED_RESET_MS          100U
/* 阶段2/3: 强制OTA指令（与 OTA boot 的 Bootloader_App.h 保持一致） */
#define BOOT_FORCE_CMD_CAN_ID            0x18FF5858UL
#define BOOT_FORCE_CMD_ENTER_BL          0xFFU
#define BOOT_FORCE_CMD_BOOT_APP2         0x02U
#define BOOT_FORCE_CMD_BOOT_APP1         0x01U
#define BOOT_FORCE_CMD_WINDOW_MS         50U

typedef enum { UDS_PHASE_IDLE=0, UDS_PHASE_ENTER_BOOTLOADER=1, UDS_PHASE_PROGRAMMING_DONE=2 } en_uds_phase_t;

typedef struct {
    uint32_t magic, phase, target_slot, fw_size, fw_crc, result, pending_sid;
    uint32_t reserved[7];
} stc_uds_shared_t;

void UdsShared_Read(stc_uds_shared_t *s);
void UdsShared_Write(const stc_uds_shared_t *s);
void UdsShared_Clear(void);
void UdsShared_SetPhase(uint32_t phase, uint32_t slot);
void App_CheckPendingUdsAck(void);

#ifdef __cplusplus
}
#endif
#endif
