#include "uds_rx_entry.h"
#include "isotp_transport.h"
#include "uds_diagnostic.h"
#include "bootloader_app.h"
#include "uds_ota.h"
#include <stdbool.h>
#include "adapter_can.h"
#include "rtt_log.h"

static uint8_t s_buf[4100];

/* 测试用：0x18FF5555 回帧确认（0x18EF5555 / data[0]=收到的指令） */
static void BootTest_SendResp(uint8_t u8Status)
{
    CanMsg_t stcMsg;

    stcMsg.u32ID = BOOT_TEST_STOP_WDT_RESP_CAN_ID;
    stcMsg.u8IDE = 1U;
    stcMsg.u8RTR = 0U;
    stcMsg.u8FDF = 0U;
    stcMsg.u8BRS = 0U;
    stcMsg.u8DLC = 8U;
    stcMsg.au8Data[0] = u8Status;
    stcMsg.u32Timestamp = 0U;
    CanIf_Send(&stcMsg);
}

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

    /* 测试用：0x18FF5555 / data[0]=0x01 → 停止喂 SWDT；0x00 → 恢复喂 SWDT（验证 boot RMU 故障计数） */
    if (can_id == BOOT_TEST_STOP_WDT_CAN_ID) {
        if (len >= 1U) {
            if (data[0] == BOOT_TEST_STOP_WDT_CMD) {
                g_swdt_feed_disable = 1U;
                MAIN_D("[TEST] SWDT feed DISABLED by 0x18FF5555\r\n");
            } else if (data[0] == BOOT_TEST_RESUME_WDT_CMD) {
                g_swdt_feed_disable = 0U;
                MAIN_D("[TEST] SWDT feed ENABLED by 0x18FF5555\r\n");
            }
            BootTest_SendResp(data[0]);   /* 回帧确认：0x18EF5555 / data[0]=收到的指令 */
        }
        return;   /* 裸帧，不进 ISOTP */
    }

    if (!is_uds_id(can_id)) return;
    uint16_t out = 0;
    if (isotp_receive_frame(0, can_id, data, len, s_buf, &out) == ISOTP_OK)
        uds_receive_handler(0, can_id, s_buf, out);
}
