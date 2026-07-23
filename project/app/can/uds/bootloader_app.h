/**
 * @file    bootloader_app.h
 * @brief   Minimal bootloader-app shared definitions for four-wheel-drive APP
 * @note    Extracted from OTA Bootloader_App.h, APP context only.
 */

#ifndef __BOOTLOADER_APP_H__
#define __BOOTLOADER_APP_H__

#include "hc32_ll.h"
#include <stdint.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/*==============================================================================
 * Flash Address Map
 *============================================================================*/
#define APP1_START_ADDR                  0x0001A000UL
#define APP2_START_ADDR                  0x0004C000UL
#define APP_RUN_SLOT_ADDR                0x0007C000UL

#define UDS_SHARED_SECTOR_BASE           0x00010000UL
#define UDS_SHARED_MAGIC                 0x55445300UL

#define UDS_TARGET_FLASH_ADDR            APP2_START_ADDR
#define UDS_POST_FLASH_BOOT_ADDR         APP1_START_ADDR
#define DELAYED_RESET_MS                 100U

/*==============================================================================
 * UDS Phase Enum
 *============================================================================*/
typedef enum {
    UDS_PHASE_IDLE              = 0,
    UDS_PHASE_ENTER_BOOTLOADER  = 1,
    UDS_PHASE_PROGRAMMING_DONE  = 2,
} en_uds_phase_t;

/*==============================================================================
 * UDS Shared State (56 bytes at 0x10000)
 *============================================================================*/
typedef struct {
    uint32_t magic;
    uint32_t phase;
    uint32_t target_slot;
    uint32_t fw_size;
    uint32_t fw_crc;
    uint32_t result;
    uint32_t pending_sid;
    uint32_t reserved[7];
} stc_uds_shared_t;

#define SLOT_A_MAGIC  0x5A5A5A5Au
#define SLOT_B_MAGIC  0xA5A5A5A5u

/*==============================================================================
 * Public Functions
 *============================================================================*/
void UdsShared_Read(stc_uds_shared_t *pState);
void UdsShared_Write(const stc_uds_shared_t *pState);
void UdsShared_Clear(void);
void UdsShared_SetPhase(uint32_t phase, uint32_t target_slot);
void App_CheckPendingUdsAck(void);

#ifdef __cplusplus
}
#endif

#endif /* __BOOTLOADER_APP_H__ */
