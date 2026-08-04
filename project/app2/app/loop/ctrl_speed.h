// /**
//  * @file    ctrl_speed.h
//  * @brief   速度环控制模块
//  */

// #ifndef __CTRL_SPEED_H
// #define __CTRL_SPEED_H

// #include "axis_typedef.h"

// void Ctrl_Speed_Run(Axis_t *axis);

// #endif



#ifndef __CTRL_SPEED_H
#define __CTRL_SPEED_H

#include "axis_typedef.h"
#include "sys_sched.h"

#ifdef __cplusplus
extern "C" {
#endif

void Ctrl_Speed_Init(void);
void Ctrl_Speed_Task(void);

#ifdef __cplusplus
}
#endif

#endif


