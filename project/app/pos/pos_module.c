/**
 * pos_module.c - Position sensor hardware layer.
 *
 * Two potentiometer instances (M1, M2) sharing ADC1_SeqB.
 * TMRA_1 triggers both SeqA (power) and SeqB (pos) simultaneously.
 */

#include "pos_module.h"
#include "sys_config.h"

#if SYS_ENABLE_POS

extern pos_handle_t pos_m1;
extern pos_handle_t pos_m2;

/** 内部 -- 环形缓冲区实例 **/
static pos_ring_buffer_t ring_buffer_pos_m1;
static pos_ring_buffer_t ring_buffer_pos_m2;

/** 外部 -- 模块硬件初始化 **/
int32_t pos_module_init(pos_handle_t *handle, const pos_cfg_t *cfg)
{
    if (handle == NULL || cfg == NULL) return -1;
    
    handle->cfg = cfg;
    const pos_hw_cfg_t *hw = cfg->hw;
    
    memset(&handle->pos_upd, 0, sizeof(pos_upd_t));
    
    // 根据句柄地址关联对应的环形缓冲区
    if (handle == &pos_m1) {
        handle->ring_buffer = &ring_buffer_pos_m1;
        memcpy(handle->pos_upd.name, "POS1", 4);
        handle->pos_upd.idx = 0;
    } else if (handle == &pos_m2) {
        handle->ring_buffer = &ring_buffer_pos_m2;
        memcpy(handle->pos_upd.name, "POS2", 4);
        handle->pos_upd.idx = 1;
    } else {
        static pos_ring_buffer_t dynamic_buffer;
        handle->ring_buffer = &dynamic_buffer;
    }
    
    // 清空环形缓冲区
    if (handle->ring_buffer) {
        memset(handle->ring_buffer, 0, sizeof(pos_ring_buffer_t));
    }
    
    // GPIO初始化...
    stc_gpio_init_t stcGpioInit;
	
    GPIO_StructInit(&stcGpioInit);
    stcGpioInit.u16PinAttr = PIN_ATTR_ANALOG;
    GPIO_Init(hw->port, hw->pin, &stcGpioInit);
    
    ADC_ChCmd(hw->ADCx, hw->seq, hw->channel, ENABLE);
    
    handle->initialized = 1;
    return 0;
}

/** 环形缓冲区 -- 写入数据 **/
int32_t pos_buffer_write(pos_handle_t *handle, float voltage)
{
    if (handle == NULL || handle->ring_buffer == NULL) return -1;
    
    pos_ring_buffer_t *rb = handle->ring_buffer;
    
    // 写入电压数据
    rb->voltage_buffer[rb->voltage_write_idx] = voltage;
    rb->voltage_write_idx = (rb->voltage_write_idx + 1) % POWER_DATA_BUFFER_SIZE;
    if (rb->voltage_count < POWER_DATA_BUFFER_SIZE) {
        rb->voltage_count++;
    }
    
    return 0;
}

/** 外部 -- 模块数据处理 放在adc中断中调用**/
void pos_module_process(pos_handle_t *handle)
{
	float voltage_pos = 0;
	if (handle == NULL) return;
    const pos_hw_cfg_t *hw = handle->cfg->hw;
	
	uint16_t adc = ADC_GetValue(hw->ADCx, hw->channel);
	voltage_pos = (adc * hw->verf / (1 << hw->adc_res)) * hw->vol_gain + hw->vol_offset;
	handle->pos_upd.cur_pos = voltage_pos;  // 保存最新值
	
	// 写入环形缓冲区
    pos_buffer_write(handle, voltage_pos);
}

/** 读取电压缓冲区数据 **/
int32_t pos_buffer_read(pos_handle_t *handle, float *data, uint16_t len, uint16_t *read_count)
{
    uint16_t i;
    uint16_t count = 0;
    
    if (handle == NULL || handle->ring_buffer == NULL || data == NULL) return -1;
    
    pos_ring_buffer_t *rb = handle->ring_buffer;
    
    count = (len < rb->voltage_count) ? len : rb->voltage_count;
    
    
	for (i = 0; i < count; i++) {
		data[i] = rb->voltage_buffer[rb->voltage_read_idx];
		rb->voltage_read_idx = (rb->voltage_read_idx + 1) % POWER_DATA_BUFFER_SIZE;
	}
	rb->voltage_count -= count;  
    
    if (read_count) *read_count = count;
    
    return 0;
}
    
#endif
