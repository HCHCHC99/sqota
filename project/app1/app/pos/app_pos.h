#ifndef __APP_POS_H__
#define __APP_POS_H__

#include "sys_module.h"
#include "pos_module.h"



/* Scheduler entry points */
void app_pos_init(void);
void app_pos_data_process(void);
void app_pos_task(void);


#endif /* __APP_POS_H__ */
