/**
 * @file    sys_state.h
 * @brief   系统状态机接口
 */
#ifndef __SYS_STATE_H
#define __SYS_STATE_H

#include "axis_typedef.h"

/**
 * @brief 系统状态枚举
 */
typedef enum {
    SYS_STATE_INIT = 0,                   // 初始化状态
    SYS_STATE_IDLE,                       // 空闲状态
    SYS_STATE_RUN,                        // 正常运行状态
    SYS_STATE_FAULT,                      // 故障状态
    SYS_STATE_EMERGENCY,                  // 紧急停止状态
    SYS_STATE_STOP,                       // 系统 停止状态
} SysState_t;

/**
 * @brief 系统状态机 初始化
 * @note  
 */
void Sys_State_Init(StateMachine_t *sm);

/**
 * @brief  系统状态机周期运行
 * @return 无
 */
void Sys_State_Task(void);

#endif


