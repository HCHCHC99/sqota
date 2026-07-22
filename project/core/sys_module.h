#ifndef __SYS_MODULE_H
#define __SYS_MODULE_H

#include <stdint.h>
#include "sys_config.h"

// 模块结构体
typedef struct {
    const char* name;             // 模块名
    void (*init)(void);           // 初始化函数
    void (*task)(void);           // 任务函数
    uint8_t prio;                 // 优先级
    uint16_t period_ms;           // 运行周期
    uint32_t last_tick;           // 上次运行时间
    uint8_t enabled;              // 模块使能标志
} SysModule_t;

// 注册模块（宏封装）
#define SYS_MODULE_REGISTER(_name, _init, _task, _prio, _period) \
    { #_name, _init, _task, _prio, _period, 0, 1 }

#endif



