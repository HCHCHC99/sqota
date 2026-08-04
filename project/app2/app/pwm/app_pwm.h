#ifndef __APP_PWM_H__
#define __APP_PWM_H__

#include "pwm_module.h"

/* Scheduler entry points */
void app_pwm_init(void);
void app_pwm_task(void);

/*
 * Debug globals ¡ª modify these in Keil watch window to test:
 *   g_test_ch   : channel index (0~7, see pwm_channel_t)
 *   g_test_duty : duty value (0 ~ PWM_PERIOD_MAX, ignored for IO channels)
 */
extern volatile pwm_channel_t g_test_ch;
extern volatile uint32_t      g_test_duty;

#endif /* __APP_PWM_H__ */
