#ifndef __HAL_ADC_H
#define __HAL_ADC_H

#include "hal_base.h"

#if HAL_ENABLE_ADC

typedef struct {
    uint8_t adc;
    uint8_t ch;
} HalADC_t;

// void Hal_ADC_Init(const HalADC_t *adc);
// uint16_t Hal_ADC_Read(const HalADC_t *adc);  // 12bit 0~4095

#endif

#endif

