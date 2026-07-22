#ifndef __HAL_PWM_H
#define __HAL_PWM_H

#include "hal_base.h"

#if HAL_ENABLE_PWM

typedef struct {
    uint8_t tim;
    uint8_t ch;
} HalPWM_t;

void Hal_PWM_Init(const HalPWM_t *pwm, uint32_t freq_hz);
void Hal_PWM_SetDuty(const HalPWM_t *pwm, uint8_t duty);  // 0~100
void Hal_PWM_Start(const HalPWM_t *pwm);
void Hal_PWM_Stop(const HalPWM_t *pwm);

#endif

#endif

