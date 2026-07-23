/**
 * @file    can_hw.c
 * @brief   CAN 硬件配置（占位实现）
 *
 * 本文件提供了一些对 CAN 硬件的简单封装占位函数。
 * 供上层模块（如 `app_can.c`）在需要时由用户替换为真实实现。
 */

#include "can_module.h"


/* 硬件配置，与之前的 app_can.c 中的 cfg_can 保持一致 */
const can_cfg_t CAN_HW = {
    .can_ins            = CAN1,
    .CANx               = CM_CAN,
    .can_bdr            = CAN_BDR_250K,
    .work_mode          = CAN_WORK_MD_NORMAL,

    .en_can_rx          = CAN_FUNC_ENABLE,
    .gpio_rx            = {
        .port               = GPIO_PORT_B,
        .pin                = GPIO_PIN_14,
        .func               = GPIO_FUNC_51,
    },
    .can_rx_cfg         = {
        .rx_warn_lmt        = 8U,
        .err_warn_lmt       = 10U,
        .rx_all_frame       = CAN_RX_ALL_FRAME_DISABLE,
        .rx_ovf_mode        = CAN_RX_OVF_SAVE_NEW,
        .self_ack           = CAN_SELF_ACK_ENABLE,
    },

    .en_can_tx          = CAN_FUNC_ENABLE,
    .gpio_tx            = {
        .port               = GPIO_PORT_B,
        .pin                = GPIO_PIN_15,
        .func               = GPIO_FUNC_50,
    },
    .can_tx_cfg         = {
        .en_ptb_single_shot = CAN_PTB_SINGLESHOT_TX_ENABLE,
        .en_stb_single_shot = CAN_STB_SINGLESHOT_TX_DISABLE,
        .en_stb_prio_md     = CAN_STB_PRIO_MD_DISABLE,
    },
    .can_int_type       = (CAN_INT_RX | CAN_INT_RX_OVERRUN | CAN_INT_RX_BUF_FULL | CAN_INT_RX_BUF_WARN | CAN_INT_ERR_INT),  // 去除 CAN_INT_PTB_TX 嫌疑②测试
    .can_int            = {
        .can_int_irqn      = INT002_IRQn,
        .can_int_pri       = DDL_IRQ_PRIO_07,
        .can_int_callback  = can_module_irq_handler,
    },
};
