/**
 * @file    bootloader_app.c
 * @brief   Minimal UDS shared state + Phase 3 deferred ACK for APP-only context
 * @note    Uses EFM_* API directly (HC32F460)
 */

#include "bootloader_app.h"
#include "hc32_ll_efm.h"
#include "adapter_can.h"
#include "log_rtt.h"

#define UDS_SECTOR_SIZE  8192U  /* 8KB */

/*==============================================================================
 * Flash helpers
 *============================================================================*/

static uint32_t read_flash_word(uint32_t addr)
{
    uint32_t val;
    EFM_ReadByte(addr, (uint8_t*)&val, 4);
    return val;
}

static void flash_erase_sector(uint32_t sector_addr)
{
    LL_PERIPH_WE(LL_PERIPH_EFM);
    EFM_FWMC_Cmd(ENABLE);
    while (SET != EFM_GetStatus(EFM_FLAG_RDY));
    EFM_ClearStatus(EFM_FLAG_ALL);
    EFM_SectorErase(sector_addr);
    while (SET != EFM_GetStatus(EFM_FLAG_RDY));
    EFM_FWMC_Cmd(DISABLE);
    LL_PERIPH_WP(LL_PERIPH_EFM);
}

static void flash_program_word(uint32_t addr, uint32_t word)
{
    LL_PERIPH_WE(LL_PERIPH_EFM);
    EFM_FWMC_Cmd(ENABLE);
    while (SET != EFM_GetStatus(EFM_FLAG_RDY));
    EFM_ProgramWordReadBack(addr, word);
    while (SET != EFM_GetStatus(EFM_FLAG_RDY));
    EFM_FWMC_Cmd(DISABLE);
    LL_PERIPH_WP(LL_PERIPH_EFM);
}

/*==============================================================================
 * UDS Shared State API (sector 0x10000)
 *============================================================================*/

void UdsShared_Read(stc_uds_shared_t *pState)
{
    if (pState == NULL) return;

    uint32_t *dst = (uint32_t*)pState;
    for (uint32_t i = 0; i < (sizeof(stc_uds_shared_t) / 4); i++) {
        /* Disable cache for direct read */
        uint32_t cache_state = EFM_CacheCmd(DISABLE);
        dst[i] = read_flash_word(UDS_SHARED_SECTOR_BASE + i * 4);
        EFM_CacheCmd(cache_state);
    }
}

void UdsShared_Write(const stc_uds_shared_t *pState)
{
    if (pState == NULL) return;

    flash_erase_sector(UDS_SHARED_SECTOR_BASE);

    const uint32_t *src = (const uint32_t*)pState;
    for (uint32_t i = 0; i < (sizeof(stc_uds_shared_t) / 4); i++) {
        flash_program_word(UDS_SHARED_SECTOR_BASE + i * 4, src[i]);
    }
}

void UdsShared_Clear(void)
{
    flash_erase_sector(UDS_SHARED_SECTOR_BASE);
}

void UdsShared_SetPhase(uint32_t phase, uint32_t target_slot)
{
    stc_uds_shared_t state;
    memset(&state, 0, sizeof(state));
    state.magic       = UDS_SHARED_MAGIC;
    state.phase       = phase;
    state.target_slot = target_slot;
    state.result      = 0;
    UdsShared_Write(&state);
}

/*==============================================================================
 * Phase 3: App_CheckPendingUdsAck
 *============================================================================*/

void App_CheckPendingUdsAck(void)
{
    stc_uds_shared_t state;
    UdsShared_Read(&state);

    if (state.magic != UDS_SHARED_MAGIC) {
        /* No pending UDS state */
        return;
    }

    LOG_INFO("[UDS] Shared state: magic=0x%08X, phase=%d, pending_sid=0x%02X",
             state.magic, state.phase, state.pending_sid);

    if (state.pending_sid == 0x11) {
        LOG_INFO("[UDS] Sending pending 11 01 ACK (51 01)");

        /* Build ISOTP single frame: PCI=0x04 + 51 01 00 00, DLC=8 */
        CanMsg_t stcMsg;
        memset(&stcMsg, 0, sizeof(stcMsg));
        stcMsg.u32ID  = 0x18DAF103UL;  /* UDS physical response ID */
        stcMsg.u8IDE  = 1;             /* Extended frame */
        stcMsg.u8DLC  = 8;
        stcMsg.au8Data[0] = 0x04;      /* ISOTP SF: 4 bytes */
        stcMsg.au8Data[1] = 0x51;      /* SID 0x11 + 0x40 */
        stcMsg.au8Data[2] = 0x01;      /* reset type */
        stcMsg.au8Data[3] = 0x00;
        stcMsg.au8Data[4] = 0x00;
        stcMsg.au8Data[5] = 0x00;
        stcMsg.au8Data[6] = 0x00;
        stcMsg.au8Data[7] = 0x00;

        CanIf_Send(&stcMsg);
    }

    /* Clear shared state */
    UdsShared_Clear();
    LOG_INFO("[UDS] Shared state cleared");
}
