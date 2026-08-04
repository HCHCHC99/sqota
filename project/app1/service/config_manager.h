#ifndef __CFG_MANAGER_H
#define __CFG_MANAGER_H

#include "sys_config.h"

#if SYS_ENABLE_CONFIG

void Cfg_Init(void);    // 初始化
void Cfg_Task(void);    // 调度运行（可选）

#endif

#endif

