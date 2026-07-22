/**
 * app_pwm.c - Scheduler hooks + debug test globals.
 *
 * Modify g_test_ch / g_test_duty in Keil's watch window during simulation.
 * app_pwm_task() applies them every tick.
 */

#include "app_pwm.h"


void app_pwm_init(void)
{
    pwm_ch_init();
	
	pwm_timera_6_start();
	
	pwm_output_enable(CH_M1_LU);
	pwm_output_enable(CH_M1_LV);
	pwm_output_enable(CH_M2_LU);
	pwm_output_enable(CH_M2_LV);
}


void app_pwm_task(void)
{

}
