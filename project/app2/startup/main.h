#ifndef __MAIN_H
#define __MAIN_H

#include "hc32_ll.h"
#include "hc32_ll_utility.h"
// #include "cmsis_armcc.h"
/*
 * 使用规则：
 * 1. 每一处ENTER必须配对EXIT，同一个变量保存状态
 * 2. 支持嵌套临界区，退出严格还原进入前全局中断状态
 * 3. 不会在退出时强行打开原本关闭的全局中断
*/

// 进入临界，保存中断状态到变量_irq_sta
#define INTERRUPT_ENABLE(_irq_sta)    do{ _irq_sta = __get_PRIMASK(); __disable_irq(); }while(0)

// 退出临界，恢复进入前的中断状态
#define INTERRUPT_DISABLE(_irq_sta)     do{ __set_PRIMASK(_irq_sta); }while(0)

#define DELAY_US(us)    do{DDL_DelayUS(us);}while (0)

#define DELAY_MS(ms)    do{DDL_DelayMS(ms);}while (0)
 
#endif

