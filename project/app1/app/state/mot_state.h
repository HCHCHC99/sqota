/**
 * @file    mot_state.h
 * @brief   电机状态机接口
 */
#ifndef __MOT_STATE_H
#define __MOT_STATE_H

#include "axis_typedef.h"
#include "hc32_ll_gpio.h"

/**
 * @brief  电机状态机初始化
 * @param  sm: 状态机指针
 * @param  evt: 事件组指针
 */
void Mot_State_Init(StateMachine_t *sm);

/**
 * @brief  电机状态机运行
 * @param  axis: 轴对象
 */
void Mot_State_Task(Axis_t *axis);

#endif


