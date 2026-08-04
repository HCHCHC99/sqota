#include "uds_rx_entry.h"
#include "isotp_transport.h"
#include "uds_diagnostic.h"
#include "bootloader_app.h"
#include "uds_ota.h"
#include <stdbool.h>

static uint8_t s_buf[4100];

static bool is_uds_id(uint32_t id) {
    return (id == 0x18DA03F1UL || id == 0x18DAF103UL ||
            id == 0x18DBFFF0UL || id == 0x18FF8118UL);
}

void uds_rx_entry(uint32_t can_id, uint8_t *data, uint8_t len) {
    /* 阶段2: 强制OTA指令（裸帧，不经过 ISOTP） */
    if (can_id == BOOT_FORCE_CMD_CAN_ID) {
        if (len >= 1U) {
            g_force_ota_cmd = data[0];
        }
        return;
    }

    if (!is_uds_id(can_id)) return;
    uint16_t out = 0;
    if (isotp_receive_frame(0, can_id, data, len, s_buf, &out) == ISOTP_OK)
        uds_receive_handler(0, can_id, s_buf, out);
}
