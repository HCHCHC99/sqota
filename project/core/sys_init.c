#include "sys_init.h"
#include "sys_sched.h"
#include "sys_tick.h"
#include "sys_config.h"
#include "log_rtt.h"
#include "msg_pubsub.h"
#include "event_group.h"
#include "fault_manager.h"
#include "app_can.h"
#include "app_pwm.h"
#include "app_power.h"

#include "power_manager.h"
#include "position_sensor.h"
#include "config_manager.h"

#include "state_engine.h"
#include "system.h"
#include "key.h"
#include "led.h"

#include "ctrl_position.h"
#include "ctrl_speed.h"
#include "ctrl_current.h"

#include "app_pos.h"
#include "flash_mcu.h"
#include "work_config.h"
#include "soft_timer.h"


// ------------------------------
// 注册默认系统模块
// ------------------------------
void System_RegisterDefaultModules(void)
{
#if SYS_ENABLE_SCHEDULER
    // 故障管理（最高优先级）
#if APP_ENABLE_FAULT_MANAGE
    SysModule_t fault_module = SYS_MODULE_REGISTER(
        "fault", Fault_Init, Fault_Task, 
        SYS_PRIO_HIGHEST, 1
    );
    Sys_Scheduler_RegisterModule(&fault_module);
#endif

#if SYS_ENABLE_SOFT_TIMER
    SysModule_t soft_timer_module = SYS_MODULE_REGISTER(
        "soft_timer", SoftTimer_GlobalInit, NULL, 
        SYS_PRIO_HIGHEST, 1
    );
    Sys_Scheduler_RegisterModule(&soft_timer_module);
#endif

    // 电源管理模块
#if SYS_ENABLE_POWER
    SysModule_t power_module = SYS_MODULE_REGISTER(
        "power", app_power_init, app_power_task, 
        SYS_PRIO_HIGH, 10
    );
    Sys_Scheduler_RegisterModule(&power_module);
#endif

/*
    // 数据存储
#if SYS_ENABLE_STORAGE
    SysModule_t storage_module = SYS_MODULE_REGISTER(
        "storage_module", bsp_storeage_init, NULL, 
        SYS_PRIO_HIGH, 10
    );
    Sys_Scheduler_RegisterModule(&storage_module);
#endif
*/
    // CAN通信模块
#if SYS_ENABLE_CAN
    SysModule_t can_module = SYS_MODULE_REGISTER(
        "CAN", app_can_init, app_can_task, 
        SYS_PRIO_HIGH, 10
    );
    Sys_Scheduler_RegisterModule(&can_module);
#endif

    // PWM 模块
#if SYS_ENABLE_PWM
    SysModule_t pwm_module = SYS_MODULE_REGISTER(
        "PWM", app_pwm_init, NULL, 
        SYS_PRIO_HIGH, 10
    );
    Sys_Scheduler_RegisterModule(&pwm_module);
#endif

    // 位置模块
#if SYS_ENABLE_POS
    SysModule_t pos_module = SYS_MODULE_REGISTER(
        "pos_module", app_pos_init, app_pos_task, 
        SYS_PRIO_HIGH, 1
    );
    Sys_Scheduler_RegisterModule(&pos_module);
#endif

    // 位置环
#if APP_ENABLE_CTRL_POSITION
    SysModule_t pos_loop_module = SYS_MODULE_REGISTER(
        "Ctrl_Pos", Ctrl_Pos_Init, Ctrl_Pos_Task, 
        SYS_PRIO_HIGH, 5
    );
    Sys_Scheduler_RegisterModule(&pos_loop_module);
#endif

    // 速度环
#if APP_ENABLE_CTRL_SPEED
    SysModule_t speed_loop_module = SYS_MODULE_REGISTER(
        "Ctrl_Speed", Ctrl_Speed_Init, Ctrl_Speed_Task, 
        SYS_PRIO_HIGH, 1
    );
    Sys_Scheduler_RegisterModule(&speed_loop_module);
#endif

    // 电流环    电流环初始化在 注册管理平台 初始化， 通过 pwm 注入 adc 转换后，通过函数指针 调用 电流环任务
#if APP_ENABLE_CTRL_CURRENT
    SysModule_t cur_loop_module = SYS_MODULE_REGISTER(
        "cur_loop", Ctrl_Curr_Init, NULL, 
        SYS_PRIO_HIGH, 1
    );
    Sys_Scheduler_RegisterModule(&cur_loop_module);
#endif

    // 消息总线（仅初始化，无任务）
#if SYS_ENABLE_MSG_PUBSUB
    SysModule_t msg_module = SYS_MODULE_REGISTER(
        "msg_bus", Msg_Init, NULL, 
        SYS_PRIO_MID, 0
    );
    Sys_Scheduler_RegisterModule(&msg_module);
#endif


    // 配置管理
#if SYS_ENABLE_CONFIG
    SysModule_t config_module = SYS_MODULE_REGISTER(
        "config", work_config_init, NULL, 
        SYS_PRIO_MID, 100
    );
    Sys_Scheduler_RegisterModule(&config_module);
#endif

    // 按钮模块
#if SYS_ENABLE_KEY
    SysModule_t key_module = SYS_MODULE_REGISTER(
        "key", Key_Init, Key_Task, 
        SYS_PRIO_MID, 1
    );
    Sys_Scheduler_RegisterModule(&key_module);
#endif

    // led模块
#if SYS_ENABLE_LED
    SysModule_t led_module = SYS_MODULE_REGISTER(
        "led", LED_Init, LED_Task, 
        SYS_PRIO_MID, 1
    );
    Sys_Scheduler_RegisterModule(&led_module);
#endif


    // 状态机模块
#if SYS_ENABLE_STATE
    SysModule_t state_machine_module = SYS_MODULE_REGISTER(
        "state_machine", State_Init, State_Task, 
        SYS_PRIO_MID, 1
    );
    Sys_Scheduler_RegisterModule(&state_machine_module);
#endif

    // 日志系统（仅初始化，无任务）
#if SYS_ENABLE_LOG_RTT
    SysModule_t log_module = SYS_MODULE_REGISTER(
        "log", Log_Init, NULL, 
        SYS_PRIO_LOWEST, 0
    );
    Sys_Scheduler_RegisterModule(&log_module);
#endif

//260613_RL_add:增加
    //机型配置模块
#if SYS_ENABLE_WORK_CGF
    SysModule_t work_cfg_module = SYS_MODULE_REGISTER(
        "work_cfg", work_config_init, NULL, 
        SYS_PRIO_LOWEST, 0
    );
    Sys_Scheduler_RegisterModule(&work_cfg_module);
#endif


#endif
}

void server_init(void){
	
	Log_Init();

}
// ------------------------------
// 系统总初始化
// ------------------------------
void System_Init(void) {
    server_init();
	
//    // 基础硬件初始化
//    mySysTick_Init();
    
    // 调度器初始化
#if SYS_ENABLE_SCHEDULER
    Sys_Scheduler_Init();
	

#endif

    // 注册默认模块
    System_RegisterDefaultModules();

    LOG_INFO("System init complete");
}

