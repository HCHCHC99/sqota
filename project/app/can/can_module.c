/** 文件包含 **/
#include "can_module.h"


/** 内部 -- GPIO初始化 **/
static void can_gpio_init(const can_gpio_t *gpio)
{
	if (gpio == NULL) return;
	
    GPIO_SetFunc(gpio->port, gpio->pin, gpio->func);   
}

/** 内部 -- 波特率 **/
static void can_cfg_bdr_init(can_bdr_t can_bdr,const can_bittime_t *bit_time, stc_can_bit_time_config_t *stcBitCfg)
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

/** 内部 -- can基础配置 **/
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
	
	//波特率
	can_cfg_bdr_init(cfg->can_bdr, &cfg->bit_time, &stcCanInit.stcBitCfg);
	
	//mode
    stcCanInit.u8WorkMode             = cfg->work_mode;
	
	//
	stcCanInit.u16FilterSelect		  = CAN_FILTER1;
	stcCanInit.pstcFilter 			  = astcFilter;
	//tx
    stcCanInit.u8PTBSingleShotTx      = cfg->can_tx_cfg.en_ptb_single_shot;
    stcCanInit.u8STBSingleShotTx      = cfg->can_tx_cfg.en_stb_single_shot;
    stcCanInit.u8STBPrioMode          = cfg->can_tx_cfg.en_stb_prio_md;
	//rx
    stcCanInit.u8RxWarnLimit          = cfg->can_rx_cfg.rx_warn_lmt;
    stcCanInit.u8ErrorWarnLimit       = cfg->can_rx_cfg.err_warn_lmt;
    stcCanInit.u8RxAllFrame           = cfg->can_rx_cfg.rx_all_frame; 
    stcCanInit.u8RxOvfMode            = cfg->can_rx_cfg.rx_ovf_mode;
	stcCanInit.u8SelfAck			  = cfg->can_rx_cfg.self_ack;
	//滤波器

//	stcCanInit.pstcFilter->u32ID	  = cfg->can_filter.id;
//	stcCanInit.pstcFilter->u32IDMask  = cfg->can_filter.id_mask;
//	stcCanInit.pstcFilter->u32IDType  = cfg->can_filter.id_type;

	
    (void)CAN_Init(CANx, &stcCanInit);
	
	//滤波器使能
//	CAN_FilterCmd(CANx, cfg->can_filter.id_type, (en_functional_state_t)cfg->en_can_filte);
	
}

/** 内部 -- 中断配置（ADC 模块可以产生以下事件输出，目前仅考虑序列A/B扫描结束事件） **/
static void can_int_cfg(CM_CAN_TypeDef *CANx, const uint32_t int_type,  const can_int_t *can_int)
{	
	stc_irq_signin_config_t stcIrq;
	
	if (can_int == NULL) return;
	if (0 == int_type) return;
	
	if (CANx == CM_CAN)	
	{
		/* IRQ sign-in */
		stcIrq.enIntSrc = INT_SRC_CAN_INT;//只有这一个
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

/** 外部 -- CAN模块初始化 **/
int32_t can_module_init(can_handle_t *handle, const can_cfg_t *cfg)
{     
	if (handle == NULL || cfg == NULL) return -1;
	
	// 复制配置
	handle->cfg_hw = cfg;
	
    handle->initialized = 0;

	/*GPIO初始化*/
    can_gpio_init(&cfg->gpio_tx);
    can_gpio_init(&cfg->gpio_rx);

	/*配置初始化*/
	can_cfg_init(handle->cfg_hw->CANx, handle->cfg_hw);
	
	/*中断配置*/
    can_int_cfg(handle->cfg_hw->CANx, handle->cfg_hw->can_int_type, &handle->cfg_hw->can_int);

	handle->initialized = 1;
    return CAN_RET_OK;
}


//can发送--标准帧
int8_t can_transmit_std(uint32_t id, uint8_t* pData, uint8_t Len)
{
	if (Len > 8)
	{
		return CAN_RET_ERR_PARAM;
	}
	
	stc_can_tx_frame_t tx;
	tx.u32Ctrl = 0x0UL;//清零，不清有问题
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
	 
	return CAN_RET_OK;
}

//can发送--扩展帧
int8_t can_transmit_ext(uint32_t id, uint8_t* pData, uint8_t Len)
{
	if (Len > 8)
	{
		return CAN_RET_ERR_PARAM;
	}
	
	stc_can_tx_frame_t tx;
	tx.u32Ctrl = 0x0UL;//清零，不清有问题
	tx.u32ID = id;
	tx.RTR = 0;//1：遥控帧  0：数据帧
	tx.IDE = 1;//1：扩展帧  0：标准帧
	tx.DLC = Len;
	
	for(uint8_t i=0; i<Len; i++)
	{
		tx.au8Data[i] = *pData;
		pData++;
	}	
	
	CAN_FillTxFrame(CM_CAN, CAN_TX_BUF_PTB, &tx);
	CAN_StartTx(CM_CAN, CAN_TX_REQ_PTB);
	 
	return CAN_RET_OK;
}


/**
 * @brief 将一帧 CAN 数据放入接收缓存（环形 FIFO）
 * @param [in] pstcCache 接收缓存指针
 * @param [in] pstcFrame 待存入的帧指针
 * @retval CAN_RET_OK 成功
 * @retval CAN_RET_ERR_INVD_PARAM 参数错误
 */
int8_t can_rx_cache_put(can_rx_cache_t *pstcCache, const stc_can_rx_frame_t *pstcFrame)
{
    if (pstcCache == NULL || pstcFrame == NULL) {
        return CAN_RET_ERR_PARAM;
    }
    
    // 存入新帧
    pstcCache->rx_frame[pstcCache->write_idx] = *pstcFrame;
    pstcCache->write_idx = (pstcCache->write_idx + 1) % CAN_RX_BUF_SIZE;
    
    if (pstcCache->cnt < CAN_RX_BUF_SIZE) {
        pstcCache->cnt++;
    } else {
        // 缓存已满，覆盖最旧帧，读指针需要同步前进
        pstcCache->read_idx = (pstcCache->read_idx + 1) % CAN_RX_BUF_SIZE;
    }
    
    return CAN_RET_OK;
}

/**
 * @brief 从接收缓存中读取一帧（先进先出，读后移除）
 * @param [in]  pstcCache 接收缓存指针
 * @param [out] pstcFrame 存放读取帧的指针
 * @retval CAN_RET_OK 成功
 * @retval CAN_RET_ERR_NODATA 缓存为空
 * @retval CAN_RET_ERR_INVD_PARAM 参数错误
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

//can接收-- 轮训方式可以放主循环里
void can_receive_poll(can_rx_cache_t *rx)
{
        stc_can_rx_frame_t frame;
    while (CAN_GetRxFrame(CM_CAN, &frame) == LL_OK) {
        can_rx_cache_put(rx, &frame);
		}
}



