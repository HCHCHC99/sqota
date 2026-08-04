#ifndef __APP_POWER_H__
#define __APP_POWER_H__

#include "power_module.h"


void app_power_init(void);
void app_power_task(void);

/*  adc采样完成中断回调，这里执行数据处理  */
void ADC1_SeqA_IrqCallback(void);
void ADC1_SeqB_IrqCallback(void);    

#endif

