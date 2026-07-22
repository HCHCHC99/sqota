/**
 * @file    can_module.c
 * @brief   CAN 硬件抽象层 (HAL) — 硬件实现
 * @note    新增 TX 完成回调、统一中断处理 (can_module_irq_handler)
 */

/** 文件包含 **/
#include "can_module.h"
#include <string.h>
#include <stdbool.h>

/*==============================================================================
 * 模块静态变量
 *============================================================================*/

/* TX 完成回调函数 */
static can_tx_callback_t m_pfnTxCallback = NULL;

/* TX 硬件忙碌标志 */
static volatile bool m_bTxBusy = false;

/* RX 缓存指针 (用于中断中快速访问) */
static can_rx_cache_t *m_pRxCache = NULL;

/*==============================================================================
 * 内部函数声明
 *============================================================================*/

static void can_gpio_init(const can_gpio_t *gpio);
static void can_cfg_bdr_init(can_bdr_t can_bdr, const can_bittime_t *bit_time,
                             stc_can_bit_time_config_t *stcBitCfg);
static void can_cfg_init(CM_CAN_TypeDef *CANx, const can_cfg_t *cfg);
static void can_int_cfg(CM_CAN_TypeDef *CANx, const uint32_t int_type,
                        const can_int_t *can_int);

/*==============================================================================
 * 内部函数实现
 *============================================================================*/

/** 内部 -- GPIO初始化 */
static void can_gpio_init(const can_gpio_t *gpio)
{
    if (gpio == NULL) return;

    GPIO_SetFunc(gpio->port, gpio->pin, gpio->func);
}

/** 内部 -- 波特率 */
static void can_cfg_bdr_init(can_bdr_t can_bdr, const can_bittime_t *bit_time,
                             stc_can_bit_time_config_t *stcBitCfg)
{
    if (stcBitCfg == NULL) return;

    if (CAN_BDR_CUSTOM == can_bdr) {
        if (bit_time == NULL) return;
        stcBitCfg->u32Prescaler = bit_time->presc;
        stcBitCfg->u32TimeSeg1  = bit_time->seg_1;
        stcBitCfg->u32TimeSeg2  = bit_time->seg_2;
        stcBitCfg->u32SJW       = bit_time->sjw;
    }
    else if (CAN_BDR_250K == can_bdr) {
        stcBitCfg->u32Prescaler = 4U;
        stcBitCfg->u32TimeSeg1  = 6U;
        stcBitCfg->u32TimeSeg2  = 2U;
        stcBitCfg->u32SJW       = 2U;

    }
    else if (CAN_BDR_500K == can_bdr) {
        stcBitCfg->u32Prescaler = 2U;
        stcBitCfg->u32TimeSeg1  = 6U;
        stcBitCfg->u32TimeSeg2  = 2U;
        stcBitCfg->u32SJW       = 2U;
    }
    else if (CAN_BDR_1M == can_bdr) {
        stcBitCfg->u32Prescaler = 1U;
        stcBitCfg->u32TimeSeg1  = 6U;
        stcBitCfg->u32TimeSeg2  = 2U;
        stcBitCfg->u32SJW       = 2U;
    }
}

/** 内部 -- can控制器配置 */
static void can_cfg_init(CM_CAN_TypeDef *CANx, const can_cfg_t *cfg)
{
    stc_can_init_t stcCanInit;

    /* Enable peripheral clock of CAN. */
    FCG_Fcg1PeriphClockCmd(FCG1_PERIPH_CAN, ENABLE);
    /* Initializes CAN. */
    (void)CAN_StructInit(&stcCanInit);

    stc_can_filter_config_t astcFilter[1] = {
        {0UL, 0x18FFFFFFUL, CAN_ID_STD_EXT}

    };

    // 波特率
    can_cfg_bdr_init(cfg->can_bdr, &cfg->bit_time, &stcCanInit.stcBitCfg);

    // mode
    stcCanInit.u8WorkMode             = cfg->work_mode;

    // 滤波器
    stcCanInit.u16FilterSelect        = CAN_FILTER1;
    stcCanInit.pstcFilter             = astcFilter;
    // tx
    stcCanInit.u8PTBSingleShotTx      = cfg->can_tx_cfg.en_ptb_single_shot;
    stcCanInit.u8STBSingleShotTx      = cfg->can_tx_cfg.en_stb_single_shot;
    stcCanInit.u8STBPrioMode          = cfg->can_tx_cfg.en_stb_prio_md;
    // rx
    stcCanInit.u8RxWarnLimit          = cfg->can_rx_cfg.rx_warn_lmt;
    stcCanInit.u8ErrorWarnLimit       = cfg->can_rx_cfg.err_warn_lmt;
    stcCanInit.u8RxAllFrame           = cfg->can_rx_cfg.rx_all_frame;
    stcCanInit.u8RxOvfMode            = cfg->can_rx_cfg.rx_ovf_mode;
    stcCanInit.u8SelfAck              = cfg->can_rx_cfg.self_ack;

    (void)CAN_Init(CANx, &stcCanInit);
}

/** 内部 -- 中断配置 */
static void can_int_cfg(CM_CAN_TypeDef *CANx, const uint32_t int_type,
                        const can_int_t *can_int)
{
    stc_irq_signin_config_t stcIrq;

    if (can_int == NULL) return;
    if (0 == int_type) return;

    if (CANx == CM_CAN)
    {
        /* IRQ sign-in */
        stcIrq.enIntSrc = INT_SRC_CAN_INT;
        stcIrq.enIRQn = can_int->can_int_irqn;
        stcIrq.pfnCallback = can_int->can_int_callback;
        (void)INTC_IrqSignIn(&stcIrq);

        /* NVIC config */
        NVIC_ClearPendingIRQ(can_int->can_int_irqn);
        NVIC_SetPriority(can_int->can_int_irqn, can_int->can_int_pri);
        NVIC_EnableIRQ(can_int->can_int_irqn);

        CAN_IntCmd(CANx, CAN_INT_ALL, DISABLE);
        CAN_IntCmd(CANx, int_type, ENABLE);
    }
}

/*==============================================================================
 * 公共 API - 初始化和收发
 *============================================================================*/

/** 外部 -- CAN模块初始化 */
int32_t can_module_init(can_handle_t *handle, const can_cfg_t *cfg)
{
    if (handle == NULL || cfg == NULL) return -1;

    /* 保存 RX 缓存指针 (供中断中快速访问) */
    m_pRxCache = &handle->can_rx;

    /* 保存配置 */
    handle->cfg_hw = cfg;

    handle->initialized = 0;

    /* 清零 RX 缓存 */
    memset(&handle->can_rx, 0, sizeof(can_rx_cache_t));

    /*GPIO初始化*/
    can_gpio_init(&cfg->gpio_tx);
    can_gpio_init(&cfg->gpio_rx);

    /*控制器初始化*/
    can_cfg_init(handle->cfg_hw->CANx, handle->cfg_hw);

    /*中断配置 (使用 can_module_irq_handler 统一处理) */
    can_int_cfg(handle->cfg_hw->CANx, handle->cfg_hw->can_int_type, &handle->cfg_hw->can_int);

    handle->initialized = 1;
    return CAN_RET_OK;
}

/*==============================================================================
 * 扩展 API — TX 回调 + 忙碌查询
 *============================================================================*/

/** 注册 TX 完成回调 */
void can_register_tx_callback(can_tx_callback_t pfnCallback)
{
    m_pfnTxCallback = pfnCallback;
}

/** 查询 TX 硬件忙碌 */
bool can_is_tx_busy(void)
{
    return m_bTxBusy;
}

/*==============================================================================
 * 发送 API
 *============================================================================*/

// can发送--标准帧
int8_t can_transmit_std(uint32_t id, uint8_t* pData, uint8_t Len)
{
    if (Len > 8)
    {
        return CAN_RET_ERR_PARAM;
    }

    stc_can_tx_frame_t tx;
    tx.u32Ctrl = 0x0UL;         // 清零，不使用控制位
    tx.u32ID = id;
    tx.RTR = 0;
    tx.IDE = 0;
    tx.DLC = Len;

    for(uint8_t i=0; i<Len; i++)
    {
        tx.au8Data[i] = *pData;
        pData++;
    }

    CAN_FillTxFrame(CM_CAN, CAN_TX_BUF_PTB, &tx);
    CAN_StartTx(CM_CAN, CAN_TX_REQ_PTB);
    m_bTxBusy = true;

    return CAN_RET_OK;
}

// can发送--扩展帧
int8_t can_transmit_ext(uint32_t id, uint8_t* pData, uint8_t Len)
{
    if (Len > 8)
    {
        return CAN_RET_ERR_PARAM;
    }

    stc_can_tx_frame_t tx;
    tx.u32Ctrl = 0x0UL;         // 清零，不使用控制位
    tx.u32ID = id;
    tx.RTR = 0;                 // 1=遥控帧  0=数据帧
    tx.IDE = 1;                 // 1=扩展帧  0=标准帧
    tx.DLC = Len;

    for(uint8_t i=0; i<Len; i++)
    {
        tx.au8Data[i] = *pData;
        pData++;
    }

    CAN_FillTxFrame(CM_CAN, CAN_TX_BUF_PTB, &tx);
    CAN_StartTx(CM_CAN, CAN_TX_REQ_PTB);
    m_bTxBusy = true;

    return CAN_RET_OK;
}

/*==============================================================================
 * 接收 API
 *============================================================================*/

/**
 * @brief 将一帧 CAN 数据放入环形缓存（尾部 FIFO）
 * @param [in] pstcCache 环形缓存指针
 * @param [in] pstcFrame 要存入的帧指针
 * @retval CAN_RET_OK 成功
 * @retval CAN_RET_ERR_PARAM 参数无效
 */
int8_t can_rx_cache_put(can_rx_cache_t *pstcCache, const stc_can_rx_frame_t *pstcFrame)
{
    if (pstcCache == NULL || pstcFrame == NULL) {
        return CAN_RET_ERR_PARAM;
    }

    // 写入帧
    pstcCache->rx_frame[pstcCache->write_idx] = *pstcFrame;
    pstcCache->write_idx = (pstcCache->write_idx + 1) % CAN_RX_BUF_SIZE;

    if (pstcCache->cnt < CAN_RX_BUF_SIZE) {
        pstcCache->cnt++;
    } else {
        // 缓冲区满，丢弃最旧帧，读指针需要同步前移
        pstcCache->read_idx = (pstcCache->read_idx + 1) % CAN_RX_BUF_SIZE;
    }

    return CAN_RET_OK;
}

/**
 * @brief 从接收缓存读取一帧（先入先出），读取后移除
 * @param [in]  pstcCache 接收缓存指针
 * @param [out] pstcFrame 存放读取帧的指针
 * @retval CAN_RET_OK 成功
 * @retval CAN_RET_ERR_NODATA 缓存为空
 * @retval CAN_RET_ERR_PARAM 参数无效
 */
int8_t can_read(can_rx_cache_t *pstcCache, stc_can_rx_frame_t *pstcFrame)
{
    if (pstcCache == NULL || pstcFrame == NULL) {
        return CAN_RET_ERR_PARAM;
    }

    if (pstcCache->cnt == 0) {
        return CAN_RET_ERR_NODATA;
    }

    *pstcFrame = pstcCache->rx_frame[pstcCache->read_idx];
    pstcCache->read_idx = (pstcCache->read_idx + 1) % CAN_RX_BUF_SIZE;
    pstcCache->cnt--;

    return CAN_RET_OK;
}

// can轮询-- 轮询式接收（主循环中调用）
void can_receive_poll(can_rx_cache_t *rx)
{
    stc_can_rx_frame_t frame;
    while (CAN_GetRxFrame(CM_CAN, &frame) == LL_OK) {
        can_rx_cache_put(rx, &frame);
    }
}

/*==============================================================================
 * 中断处理 (统一入口, 注册到中断控制器)
 *============================================================================*/

/**
 * @brief CAN 中断统一处理函数
 * @note  在 CAN 中断上下文中调用，处理 RX 接收、TX 完成、错误/Bus-Off 恢复
 *        此函数替代了原先分散在 app_can.c 中的应用层 ISR
 */
void can_module_irq_handler(void)
{
    CM_CAN_TypeDef *CANx = CM_CAN;
    uint32_t status = 0;

    /* 接收中断: 将硬件 FIFO 中所有帧取出放入软件缓存 */
    if (CAN_GetStatus(CANx, CAN_FLAG_RX) == SET) {
        status |= CAN_FLAG_RX;
        stc_can_rx_frame_t frame;
        while (CAN_GetRxBufStatus(CANx) != CAN_RX_BUF_EMPTY) {
            while (CAN_GetRxFrame(CM_CAN, &frame) == LL_OK)
            {
                if (m_pRxCache != NULL) {
                    can_rx_cache_put(m_pRxCache, &frame);
                }
            }
        }
    }

    /* 接收溢出 */
    if (CAN_GetStatus(CANx, CAN_FLAG_RX_OVERRUN) == SET) {
        status |= CAN_FLAG_RX_OVERRUN;
    }

    /* 接收缓冲满告警 */
    if (CAN_GetStatus(CANx, CAN_FLAG_RX_BUF_FULL) == SET) {
        status |= CAN_FLAG_RX_BUF_FULL;
    }

    /* 接收缓冲警告 */
    if (CAN_GetStatus(CANx, CAN_FLAG_RX_BUF_WARN) == SET) {
        status |= CAN_FLAG_RX_BUF_WARN;
    }

    /* PTB 发送完成 */
    if (CAN_GetStatus(CANx, CAN_FLAG_PTB_TX) == SET) {
        status |= CAN_FLAG_PTB_TX;
        m_bTxBusy = false;
        if (m_pfnTxCallback != NULL) {
            m_pfnTxCallback();
        }
    }

    /* STB 发送完成 */
    if (CAN_GetStatus(CANx, CAN_FLAG_STB_TX) == SET) {
        status |= CAN_FLAG_STB_TX;
    }

    /* 错误中断 / Bus-Off */
    if (CAN_GetStatus(CANx, CAN_FLAG_ERR_INT) == SET) {
        if (CAN_GetStatus(CANx, CAN_FLAG_BUS_OFF) == SET)
        {
            CAN_ExitLocalReset(CANx);
            status |= CAN_FLAG_BUS_OFF;
        }
        status |= CAN_FLAG_ERR_INT;
    }

    /* 总线错误 */
    if (CAN_GetStatus(CANx, CAN_FLAG_BUS_ERR) == SET) {
        status |= CAN_FLAG_BUS_ERR;
    }

    /* 清除所有已处理的中断标志 */
    if (status != 0) {
        CAN_ClearStatus(CANx, status);
    }
}
