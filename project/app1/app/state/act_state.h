/**
 * @file    act_state.h
 * @brief   推杆状态机接口
 */
#ifndef __ACT_STATE_H
#define __ACT_STATE_H

#include "axis_typedef.h"

/**
 * @brief 推杆状态枚举
 */
typedef enum {
    ACT_STATE_INIT = 0,                   // 初始化状态
    ACT_STATE_IDLE ,                      // 空闲状态
   
    ACT_STATE_COMBINE,
    ACT_STATE_COMBINE_TRY_1,
    ACT_STATE_COMBINE_TRY_2,
    ACT_STATE_COMBINE_TRY_3,
    ACT_STATE_COMBINE_BACK,
    ACT_STATE_COMBINE_BACK_1,
    ACT_STATE_COMBINE_BACK_2,
    ACT_STATE_COMBINE_SUCCESS,
    ACT_STATE_COMBINE_FAIL,

    ACT_STATE_SEPARATE,
    ACT_STATE_SEPARATE_TRY_1,
    ACT_STATE_SEPARATE_TRY_2,
    ACT_STATE_SEPARATE_TRY_3,
    ACT_STATE_SEPARATE_BACK,
    ACT_STATE_SEPARATE_BACK_1,
    ACT_STATE_SEPARATE_BACK_2,
    ACT_STATE_SEPARATE_SUCCESS,
    ACT_STATE_SEPARATE_FAIL,


    ACT_STATE_DISABLE,
    ACT_STATE_HOLD,                       // 保持/制动/停止
    ACT_STATE_ERROR,                      // 错误

    ACT_STATE_CALIBRATION = 22,             //260616_RL_add：标定  
    ACT_STATE_CALIBRATION_OUT,              //260616_RL_add：标定伸出
    ACT_STATE_CALIBRATION_IN,               //260616_RL_add：标定伸出
} ActuatorState_t;

/**
 * @brief  推杆状态机初始化
 * @param  sm: 状态机指针
 * @param  evt: 事件组指针
 */
void Act_State_Init(StateMachine_t *sm);

/**
 * @brief  推杆状态机运行
 * @param  axis: 推杆对象指针
 */
void Act_State_Task(Axis_t *axis);

/*
 * //260606_RL_add:
 * @brief  推杆遇阻处理函数
 * @param  axis: 遇阻标志指针
 */
void act_babk_handler(uint8_t* MotorBackFlag);

#endif


