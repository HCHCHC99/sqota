#include "flash_download.h"
#include "hc32_ll.h"
#include "hc32_ll_efm.h"
#include "rtt_log.h"
#include <string.h>

#define FW_D(fmt, ...)  LOG_CH(LOG_CH_MAIN, LOG_LEVEL_DEBUG, COLOR_CYAN,   "FW", fmt, ##__VA_ARGS__)
#define FW_I(fmt, ...)  LOG_CH(LOG_CH_MAIN, LOG_LEVEL_INFO,  COLOR_GREEN, "FW", fmt, ##__VA_ARGS__)
#define FW_W(fmt, ...)  LOG_CH(LOG_CH_MAIN, LOG_LEVEL_WARN,  COLOR_YELLOW,"FW", fmt, ##__VA_ARGS__)
#define FW_E(fmt, ...)  LOG_CH(LOG_CH_MAIN, LOG_LEVEL_ERROR, COLOR_RED,   "FW", fmt, ##__VA_ARGS__)

typedef struct {
    FlashDownloadState_t  state;
    FlashDownloadResult_t last_error;
    uint32_t target_address, total_size, received_size;
    uint8_t  expected_sequence;
    FlashDownloadConfig_t config;
    bool     pending_response;
    uint32_t rx_crc;
} FlashDownloadContext_t;

static FlashDownloadContext_t g_ctx;
static bool g_inited;
static uint8_t g_buf[FW_RAM_BUFFER_SIZE] __attribute__((aligned(4)));

static void set_state(FlashDownloadState_t s) {
    if (g_ctx.state != s) { FW_I("State: %d -> %d", g_ctx.state, s); g_ctx.state = s; }
}
static uint32_t sector_start(uint32_t a) { return a & ~(g_ctx.config.flash_sector_size - 1); }

static uint32_t crc32_byte(uint32_t crc, uint8_t b) {
    crc ^= b; for (int j = 0; j < 8; j++) crc = (crc & 1) ? ((crc >> 1) ^ 0xEDB88320) : (crc >> 1);
    return crc;
}
static uint32_t flash_crc32(uint32_t addr, uint32_t size) {
    uint32_t crc = 0xFFFFFFFF; uint8_t buf[256];
    while (size) {
        uint32_t n = (size > 256) ? 256 : size;
        EFM_ReadByte(addr, buf, n);
        for (uint32_t i = 0; i < n; i++) crc = crc32_byte(crc, buf[i]);
        addr += n; size -= n;
    }
    return ~crc;
}
static int32_t flash_erase_sector(uint32_t addr) {
    LL_PERIPH_WE(LL_PERIPH_EFM); EFM_FWMC_Cmd(ENABLE);
    while (SET != EFM_GetStatus(EFM_FLAG_RDY)); EFM_ClearStatus(EFM_FLAG_ALL);
    int32_t r = EFM_SectorErase(addr);
    while (SET != EFM_GetStatus(EFM_FLAG_RDY));
    EFM_FWMC_Cmd(DISABLE); LL_PERIPH_WP(LL_PERIPH_EFM);
    return r;
}
static int32_t flash_write_word(uint32_t addr, uint32_t val) {
    LL_PERIPH_WE(LL_PERIPH_EFM); EFM_FWMC_Cmd(ENABLE);
    while (SET != EFM_GetStatus(EFM_FLAG_RDY));
    int32_t r = EFM_ProgramWordReadBack(addr, val);
    while (SET != EFM_GetStatus(EFM_FLAG_RDY));
    EFM_FWMC_Cmd(DISABLE); LL_PERIPH_WP(LL_PERIPH_EFM);
    return r;
}

static FlashDownloadResult_t erase_range(uint32_t s, uint32_t e) {
    uint32_t sz = g_ctx.config.flash_sector_size;
    FW_I("Erasing range: 0x%08X - 0x%08X", sector_start(s), sector_start(e));
    for (uint32_t a = sector_start(s); a <= sector_start(e); a += sz) {
        if (flash_erase_sector(a) != 0) { FW_E("Erase fail 0x%08X", a); return FW_RESULT_ERASE_FAILED; }
    }
    return FW_RESULT_OK;
}
static FlashDownloadResult_t write_buf(uint32_t addr, const uint8_t* buf, uint32_t size) {
    uint32_t wc = (size + 3) / 4;
    memcpy(g_buf, buf, size); for (uint32_t i = size; i < wc * 4; i++) g_buf[i] = 0xFF;
    FW_D("Writing to Flash: addr=0x%08X, size=%d, words=%d", addr, size, wc);
    const uint32_t* src = (const uint32_t*)g_buf;
    for (uint32_t i = 0; i < wc; i++) {
        if (flash_write_word(addr + i * 4, src[i]) != 0) {
            FW_E("Write fail 0x%08X", addr + i * 4); return FW_RESULT_WRITE_FAILED;
        }
    }
    return FW_RESULT_OK;
}

void FlashDownload_Init(const FlashDownloadConfig_t* cfg) {
    memset(&g_ctx, 0, sizeof(g_ctx));
    g_ctx.config.max_firmware_size = FW_APP_MAX_SIZE; g_ctx.config.flash_sector_size = 0x2000;
    g_ctx.config.user_start_addr = FW_APP_START_ADDR;
    g_ctx.config.user_end_addr = FW_APP_START_ADDR + FW_APP_MAX_SIZE - 1;
    g_ctx.config.verify_enabled = 1;
    if (cfg) {
        if (cfg->max_firmware_size) g_ctx.config.max_firmware_size = cfg->max_firmware_size;
        if (cfg->flash_sector_size) g_ctx.config.flash_sector_size = cfg->flash_sector_size;
    }
    set_state(FW_UPDATE_IDLE); g_inited = true;
    FW_I("FlashDownload init done: start=0x%08X, max_size=%d, sector=%d",
         g_ctx.config.user_start_addr, g_ctx.config.max_firmware_size, g_ctx.config.flash_sector_size);
}

FlashDownloadResult_t FlashDownload_OnRequestDownload(uint32_t addr, uint32_t size) {
    if (!g_inited) return FW_RESULT_NOT_READY;
    if (g_ctx.state != FW_UPDATE_IDLE) { FW_W("Busy, state=%d", g_ctx.state); return FW_RESULT_BUSY; }
    uint32_t m = MAP_TBOX_ADDR_TO_FLASH(addr);
    if (m < FW_APP_START_ADDR || m >= FW_APP_START_ADDR + FW_APP_MAX_SIZE) {
        FW_E("Download address invalid: 0x%08X -> 0x%08X", addr, m); return FW_RESULT_ADDR_INVALID;
    }
    if (size > g_ctx.config.max_firmware_size || !size) {
        FW_E("Size invalid: %d", size); return FW_RESULT_SIZE_TOO_LARGE;
    }
    if (m + size > FW_APP_START_ADDR + FW_APP_MAX_SIZE) {
        FW_E("Range invalid: 0x%08X + %d exceeds APP2 window", m, size);
        return FW_RESULT_ADDR_INVALID;
    }
    FlashDownloadResult_t r = erase_range(m, m + size - 1);
    if (r != FW_RESULT_OK) return r;
    g_ctx.target_address = m; g_ctx.total_size = size; g_ctx.received_size = 0;
    g_ctx.expected_sequence = 1; g_ctx.rx_crc = 0xFFFFFFFF;
    FW_I("Download ready: addr=0x%08X, size=%d", m, size); set_state(FW_UPDATE_READY);
    return FW_RESULT_OK;
}

FlashDownloadResult_t FlashDownload_OnTransferData(uint8_t seq, uint8_t* data, uint16_t len) {
    if (!g_inited) return FW_RESULT_NOT_READY;
    if (g_ctx.state != FW_UPDATE_READY && g_ctx.state != FW_UPDATE_TRANSFERRING) {
        FW_W("Not ready, state=%d", g_ctx.state); return FW_RESULT_SEQUENCE_ERROR;
    }
    if (!len) return FW_RESULT_OK;
    if (seq != g_ctx.expected_sequence) {
        FW_E("Seq mismatch: exp %d got %d", g_ctx.expected_sequence, seq);
        g_ctx.last_error = FW_RESULT_SEQUENCE_ERROR; set_state(FW_UPDATE_ERROR);
        return FW_RESULT_SEQUENCE_ERROR;
    }
    if (len > g_ctx.total_size - g_ctx.received_size) return FW_RESULT_SIZE_TOO_LARGE;
    set_state(FW_UPDATE_TRANSFERRING);
    FlashDownloadResult_t r = write_buf(g_ctx.target_address + g_ctx.received_size, data, len);
    if (r != FW_RESULT_OK) { g_ctx.last_error = r; set_state(FW_UPDATE_ERROR); return r; }
    for (uint16_t i = 0; i < len; i++) g_ctx.rx_crc = crc32_byte(g_ctx.rx_crc, data[i]);
    g_ctx.received_size += len; g_ctx.expected_sequence++;
    FW_D("Block %d: %d bytes, total=%d/%d", seq, len, g_ctx.received_size, g_ctx.total_size);
    return FW_RESULT_OK;
}

FlashDownloadResult_t FlashDownload_OnTransferExit(void) {
    if (!g_inited) return FW_RESULT_NOT_READY;
    g_ctx.rx_crc = ~g_ctx.rx_crc;
    FW_I("Transfer exit: received=%d/%d, CRC=0x%08X", g_ctx.received_size, g_ctx.total_size, g_ctx.rx_crc);
    if (g_ctx.received_size < g_ctx.total_size)
        FW_W("Incomplete: %d < %d", g_ctx.received_size, g_ctx.total_size);
    if (g_ctx.config.verify_enabled) {
        set_state(FW_UPDATE_VERIFYING);
        uint32_t fcrc = flash_crc32(g_ctx.target_address, g_ctx.total_size);
        FW_I("Verify: flash_crc=0x%08X, rx_crc=0x%08X", fcrc, g_ctx.rx_crc);
        if (fcrc != g_ctx.rx_crc) { FW_E("CRC mismatch!"); set_state(FW_UPDATE_ERROR); return FW_RESULT_VERIFY_FAILED; }
    }
    set_state(FW_UPDATE_COMPLETE); FW_I("Flash download complete!");
    return FW_RESULT_OK;
}

FlashDownloadResult_t FlashDownload_Erase(uint32_t a, uint32_t s) { return erase_range(MAP_TBOX_ADDR_TO_FLASH(a), MAP_TBOX_ADDR_TO_FLASH(a) + s - 1); }
FlashDownloadResult_t FlashDownload_CalculateCRC(uint32_t a, uint32_t s, uint32_t* o) { if (!o||!s) return FW_RESULT_ADDR_INVALID; *o = flash_crc32(MAP_TBOX_ADDR_TO_FLASH(a), s); return FW_RESULT_OK; }
FlashDownloadState_t FlashDownload_GetState(void) { return g_ctx.state; }
void FlashDownload_GetProgress(FlashDownloadProgress_t* p) {
    if (!p) return; p->total_size = g_ctx.total_size; p->received_size = g_ctx.received_size;
    p->target_address = g_ctx.target_address;
    p->progress_percent = g_ctx.total_size ? (uint8_t)((g_ctx.received_size * 100) / g_ctx.total_size) : 0;
}
FlashDownloadResult_t FlashDownload_GetLastError(void) { return g_ctx.last_error; }
uint16_t FlashDownload_GetFirmwareVersion(void) { uint32_t w; EFM_ReadByte(FW_APP_START_ADDR, (uint8_t*)&w, 4); return (uint16_t)w; }
uint16_t FlashDownload_GetBootloaderVersion(void) { uint32_t w; EFM_ReadByte(0, (uint8_t*)&w, 4); return (uint16_t)w; }
uint32_t FlashDownload_GetFirmwareCRC(void) { uint32_t c; EFM_ReadByte(FW_APP_START_ADDR, (uint8_t*)&c, 4); return c; }
void FlashDownload_Cancel(void) { set_state(FW_UPDATE_IDLE); g_ctx.pending_response = false; FW_I("Download cancelled"); }
void FlashDownload_Reset(void) { memset(&g_ctx, 0, sizeof(g_ctx)); g_inited = false; }
void FlashDownload_Task(void) {}
bool FlashDownload_IsPending(void) { return g_ctx.pending_response; }
