/**
 * @file    uds_rx_entry.h
 * @brief   UDS RX entry point from body CAN protocol's default branch
 */

#ifndef __UDS_RX_ENTRY_H__
#define __UDS_RX_ENTRY_H__

#include <stdint.h>

/**
 * @brief Try to process a CAN frame as UDS protocol.
 *        Called from app_can_receive() when body CAN protocol doesn't match.
 * @param can_id  29-bit CAN ID
 * @param data    8-byte data buffer
 * @param len     DLC (data length)
 */
void uds_rx_entry(uint32_t can_id, uint8_t *data, uint8_t len);

#endif /* __UDS_RX_ENTRY_H__ */
