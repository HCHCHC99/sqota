// /**
//  * @file    ctrl_position.h
//  * @brief   位置环控制模块
//  * @note    10ms调用，纯算法，无硬件
//  */

// #ifndef __CTRL_POSITION_H
// #define __CTRL_POSITION_H

// #include "axis_typedef.h"

// /**
//  * @brief  位置环运行
//  * @param  axis: 轴对象指针
//  * @return 无
//  */
// void Ctrl_Position_Run(Axis_t *axis);

// #endif


#ifndef __CTRL_POSITION_H
#define __CTRL_POSITION_H

#include "axis_typedef.h"
#include "sys_sched.h"

#ifdef __cplusplus
extern "C" {
#endif

void Ctrl_Pos_Init(void);
void Ctrl_Pos_Task(void);

#ifdef __cplusplus
}
#endif

#endif


