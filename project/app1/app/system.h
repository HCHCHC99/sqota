/**
 * @file    system.h
 * @brief   系统初始化与调度接口
 */
#ifndef __SYSTEM_H
#define __SYSTEM_H

#include "axis_typedef.h"
#include "hc32_ll.h"

/**
 * @brief  系统全局初始化
 * @return 无
 */
void State_Init(void);

/**
 * @brief  系统主任务调度（周期调用）
 * @return 无
 */
void State_Task(void);

#endif


