#include "sys_tick.h"
#include "hc32_ll_utility.h"
//static volatile uint32_t sys_tick = 0;

//void mySysTick_Init(void) {
//   sys_tick = 0;
//   // 底层定时器1ms初始化（由hal_timer实现）
//}

uint32_t mySysTick_Get(void) {
   return SysTick_GetTick();
}

//void mySysTick_Inc(void) {
//   sys_tick++;
//}

