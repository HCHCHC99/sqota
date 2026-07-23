/**
 * @file    uds_rx_entry.c
 * @brief   UDS RX entry — filters CAN IDs, passes UDS frames to ISOTP/UDS stack
 * @note    Layer 2 protocol: only processes UDS-specific CAN IDs,
 *          discards all others (body protocol handled these already).
 */

#include "uds_rx_entry.h"
#include "isotp_transport.h"
#include "uds_diagnostic.h"
#include <stdbool.h>

/* ISOTP reassembly buffer (max 4096 bytes per ISO 15765-2) */
static uint8_t s_uds_rx_buffer[4100];

/* UDS CAN IDs recognized by this entry */
static bool is_uds_can_id(uint32_t can_id)
{
    switch (can_id) {
        case 0x18DA03F1UL:   /* UDS physical request (TBOX -> ECU) */
        case 0x18DAF103UL:   /* UDS physical response (ECU -> TBOX) */
        case 0x18DBFFF0UL:   /* UDS functional request (broadcast) */
        case 0x18FF8118UL:   /* OTA dedicated ID */
            return true;
        default:
            return false;
    }
}

void uds_rx_entry(uint32_t can_id, uint8_t *data, uint8_t len)
{
    /* Filter: only process UDS CAN IDs */
    if (!is_uds_can_id(can_id)) {
        return;  /* Not a UDS frame → discard */
    }

    /* Feed single CAN frame into ISOTP reassembly */
    uint16_t out_len = 0;
    int8_t result = isotp_receive_frame(0, can_id, data, len,
                                        s_uds_rx_buffer, &out_len);
    if (result == ISOTP_OK) {
        /* Complete UDS message received → dispatch to UDS handler */
        uds_receive_handler(0, can_id, s_uds_rx_buffer, out_len);
    }
    /* If ISOTP_BUSY: multi-frame in progress, wait for more frames */
}
