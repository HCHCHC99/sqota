#include "bootloader_app.h"
#include "hc32_ll_efm.h"
#include "adapter_can.h"
#include "rtt_log.h"

static void erase_sector(uint32_t addr) {
    LL_PERIPH_WE(LL_PERIPH_EFM); EFM_FWMC_Cmd(ENABLE);
    while (SET != EFM_GetStatus(EFM_FLAG_RDY)); EFM_ClearStatus(EFM_FLAG_ALL);
    EFM_SectorErase(addr);
    while (SET != EFM_GetStatus(EFM_FLAG_RDY));
    EFM_FWMC_Cmd(DISABLE); LL_PERIPH_WP(LL_PERIPH_EFM);
}
static void prog_word(uint32_t addr, uint32_t word) {
    LL_PERIPH_WE(LL_PERIPH_EFM); EFM_FWMC_Cmd(ENABLE);
    while (SET != EFM_GetStatus(EFM_FLAG_RDY));
    EFM_ProgramWordReadBack(addr, word);
    while (SET != EFM_GetStatus(EFM_FLAG_RDY));
    EFM_FWMC_Cmd(DISABLE); LL_PERIPH_WP(LL_PERIPH_EFM);
}
static uint32_t read_word(uint32_t addr) { uint32_t v; EFM_ReadByte(addr, (uint8_t*)&v, 4); return v; }

void UdsShared_Read(stc_uds_shared_t *s) {
    if (!s) return;
    uint32_t *d = (uint32_t*)s;
    for (uint32_t i = 0; i < sizeof(stc_uds_shared_t)/4; i++)
        d[i] = read_word(UDS_SHARED_SECTOR_BASE + i*4);
}
void UdsShared_Write(const stc_uds_shared_t *s) {
    if (!s) return;
    erase_sector(UDS_SHARED_SECTOR_BASE);
    const uint32_t *src = (const uint32_t*)s;
    for (uint32_t i = 0; i < sizeof(stc_uds_shared_t)/4; i++)
        prog_word(UDS_SHARED_SECTOR_BASE + i*4, src[i]);
}
void UdsShared_Clear(void) { erase_sector(UDS_SHARED_SECTOR_BASE); }
void UdsShared_SetPhase(uint32_t phase, uint32_t slot) {
    stc_uds_shared_t s; memset(&s, 0, sizeof(s));
    s.magic = UDS_SHARED_MAGIC; s.phase = phase; s.target_slot = slot;
    UdsShared_Write(&s);
}
void App_CheckPendingUdsAck(void) {
    stc_uds_shared_t s; UdsShared_Read(&s);
    if (s.magic != UDS_SHARED_MAGIC) return;
    MAIN_D("UDS Shared state: magic=0x%08X, phase=%d, pending_sid=0x%02X\r\n",
           s.magic, s.phase, s.pending_sid);
    if (s.pending_sid == 0x11) {
        MAIN_D("Sending pending 11 01 ACK (51 01)\r\n");
        CanMsg_t m; memset(&m, 0, sizeof(m));
        m.u32ID = 0x18DAF103UL; m.u8IDE = 1; m.u8DLC = 8;
        m.au8Data[0]=0x04; m.au8Data[1]=0x51; m.au8Data[2]=0x01;
        CanIf_Send(&m);
    }
    UdsShared_Clear();
    MAIN_D("UDS Shared state cleared\r\n");
}
