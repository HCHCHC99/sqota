/**
 * @file    sys_state.c
 * @brief   系统状态机实现
 * @note    负责：控制器整机、按键、LED、全局故障、急停管理
 */
#include "sys_state.h"
#include "event_def.h"
#include "stddef.h"
#include "led.h"

/* 状态入口函数声明 */
static void sys_enter_init(void);
static void sys_enter_idle(void);
static void sys_enter_run(void);
static void sys_enter_stop(void);
static void sys_enter_fault(void);
static void sys_enter_emergency(void);


/**
 * @brief 系统状态跳转表
 */
static const StateJumpTable_t sys_jump[] = {
    {SYS_STATE_INIT,        EVT_SYS_INIT_DONE,             SYS_STATE_IDLE},           // 系统初始化 --> 系统空闲

    {SYS_STATE_IDLE,        EVT_SYS_CMD_WORK_ENABLE,       SYS_STATE_RUN},            // 系统空闲 --> 系统运行
    {SYS_STATE_IDLE,        EVT_SYS_FAULT,                 SYS_STATE_FAULT},
    {SYS_STATE_IDLE,        EVT_SYS_EMERGENCY,             SYS_STATE_EMERGENCY},
    {SYS_STATE_IDLE,        EVT_SYS_ST_WORK_ERROR,         SYS_STATE_STOP},
    {SYS_STATE_IDLE,        EVT_SYS_VOLT_OVER,         	   SYS_STATE_FAULT},
    {SYS_STATE_IDLE,        EVT_SYS_VOLT_UNDER,            SYS_STATE_FAULT},

    {SYS_STATE_RUN,         EVT_SYS_FAULT,                 SYS_STATE_FAULT},
    {SYS_STATE_RUN,         EVT_SYS_EMERGENCY,             SYS_STATE_EMERGENCY},
    {SYS_STATE_RUN,         EVT_SYS_ST_WORK_ERROR,         SYS_STATE_STOP},
    {SYS_STATE_RUN,         EVT_SYS_VOLT_OVER,         	   SYS_STATE_FAULT},
    {SYS_STATE_RUN,         EVT_SYS_VOLT_UNDER,            SYS_STATE_FAULT},

    {SYS_STATE_STOP,        EVT_SYS_CMD_WORK_ENABLE,       SYS_STATE_RUN},
    {SYS_STATE_STOP,        EVT_SYS_RECOVERY,              SYS_STATE_IDLE},

    {SYS_STATE_FAULT,       EVT_SYS_RECOVERY,              SYS_STATE_IDLE},
    {SYS_STATE_FAULT,       EVT_SYS_RECOVERY,              SYS_STATE_IDLE},

    {SYS_STATE_EMERGENCY,   EVT_SYS_RECOVERY,              SYS_STATE_IDLE},
};

/**
 * @brief 系统状态入口函数表
 */
static const StateFuncTable_t sys_func[] = {
    {SYS_STATE_INIT,      sys_enter_init,       NULL},
    {SYS_STATE_IDLE,      sys_enter_idle,       NULL},
    {SYS_STATE_RUN,       sys_enter_run,        NULL},
    {SYS_STATE_STOP,      sys_enter_stop,       NULL},
    {SYS_STATE_FAULT,     sys_enter_stop,       NULL},
    {SYS_STATE_EMERGENCY, sys_enter_stop,       NULL},
};

/**
 * @brief 系统状态机 初始化
 * @note  
 */
void Sys_State_Init(StateMachine_t *sm)
{
    StateMachine_t *sm_temp = sm;
    sm_temp->jump_table = sys_jump;
    sm_temp->jump_table_size = sizeof(sys_jump)/sizeof(StateJumpTable_t);
    sm_temp->func_table = sys_func;
    sm_temp->func_table_size = sizeof(sys_func)/sizeof(StateFuncTable_t);
    sm_temp->init_state = SYS_STATE_INIT;

    StateMachine_Init(sm_temp);
}
/**
 * @brief 系统状态机运行函数
 * @note  读取系统事件组，驱动状态机流转
 */
void Sys_State_Task(void)
{
    EventBits_t bits = EventGroup_Get(mySystem.sys_evt);
	
   if (EventGroup_Recv(mySystem.sys_evt , 
                        EVT_SYS_INIT_DONE | EVT_SYS_CMD_WORK_ENABLE , 
                        EVENT_RECV_AND , 10))
   {
        StateMachine_SendEvent(&mySystem.sys_sm, EVT_SYS_INIT_DONE);
        StateMachine_SendEvent(&mySystem.sys_sm, EVT_SYS_CMD_WORK_ENABLE);
   }

   if (EventGroup_Recv(mySystem.sys_evt , 
                        EVT_SYS_INIT_DONE | EVT_ACT_WORK_ENABLE , 
                        EVENT_RECV_AND , 10))
   {
        StateMachine_SendEvent(&mySystem.sys_sm, EVT_SYS_INIT_DONE);
        StateMachine_SendEvent(&mySystem.sys_sm, EVT_ACT_WORK_ENABLE);
   }

   if (EventGroup_Recv(mySystem.sys_evt , 
                        EVT_SYS_RECOVERY | EVT_SYS_CMD_WORK_ENABLE , 
                        EVENT_RECV_AND , 10))
   {
        StateMachine_SendEvent(&mySystem.sys_sm, EVT_SYS_RECOVERY);
        StateMachine_SendEvent(&mySystem.sys_sm, EVT_SYS_CMD_WORK_ENABLE);
   }
	
    switch (bits)
    {
    case EVT_SYS_INIT_DONE:        // 初始化完成 事件
        StateMachine_SendEvent(&mySystem.sys_sm, EVT_SYS_INIT_DONE);
        EventGroup_Clear(mySystem.sys_evt, EVT_SYS_INIT_DONE);
        break;
    case EVT_SYS_CMD_WORK_ENABLE:  // 工作使能 事件
        StateMachine_SendEvent(&mySystem.sys_sm, EVT_SYS_CMD_WORK_ENABLE);
        EventGroup_Clear(mySystem.sys_evt, EVT_SYS_CMD_WORK_ENABLE);
        break;
    case EVT_SYS_FAULT:            // 系统故障 事件
        StateMachine_SendEvent(&mySystem.sys_sm, EVT_SYS_FAULT);
        EventGroup_Clear(mySystem.sys_evt, EVT_SYS_FAULT);
        break;
    case EVT_SYS_VOLT_OVER:        // 系统故障 过压
        StateMachine_SendEvent(&mySystem.sys_sm, EVT_SYS_VOLT_OVER);
        EventGroup_Clear(mySystem.sys_evt, EVT_SYS_VOLT_OVER);
        break;
    case EVT_SYS_VOLT_UNDER:       // 系统故障 欠压
        StateMachine_SendEvent(&mySystem.sys_sm, EVT_SYS_VOLT_UNDER);
        EventGroup_Clear(mySystem.sys_evt, EVT_SYS_VOLT_UNDER);
        break;
    case EVT_SYS_VOLT_NORMAL:       // 系统电压正常
        StateMachine_SendEvent(&mySystem.sys_sm, EVT_SYS_VOLT_NORMAL);
        EventGroup_Clear(mySystem.sys_evt, EVT_SYS_VOLT_NORMAL);
        break;
    case EVT_SYS_EMERGENCY:        // 急停 事件
        StateMachine_SendEvent(&mySystem.sys_sm, EVT_SYS_EMERGENCY);
        EventGroup_Clear(mySystem.sys_evt, EVT_SYS_EMERGENCY);
        break;
    case EVT_SYS_ST_WORK_ERROR:    // 工作错误 事件
        StateMachine_SendEvent(&mySystem.sys_sm, EVT_SYS_ST_WORK_ERROR);
        EventGroup_Clear(mySystem.sys_evt, EVT_SYS_ST_WORK_ERROR);
        break;
    case EVT_SYS_RECOVERY:         // 故障恢复 事件
        StateMachine_SendEvent(&mySystem.sys_sm, EVT_SYS_RECOVERY);
        EventGroup_Clear(mySystem.sys_evt, EVT_SYS_RECOVERY);
        break;

    default:
        break;
    }
}

/* 状态入口函数实现 */
static void sys_enter_init(void)      {}
static void sys_enter_idle(void)      {}
static void sys_enter_run(void)       {
    EventGroup_Send(mySystem.axis[0].evt_act, EVT_ACT_WORK_ENABLE);
    EventGroup_Send(mySystem.axis[1].evt_act, EVT_ACT_WORK_ENABLE);
		
    EventGroup_Send(mySystem.axis[0].evt_can, EVT_CAN_WORK_ENABLE);
    EventGroup_Send(mySystem.axis[1].evt_can, EVT_CAN_WORK_ENABLE);
	
	//还有灯失能
	led_set_event(0, EVT_LEDx_ENABLE);
	led_set_event(1, EVT_LEDx_ENABLE);
}
static void sys_enter_stop(void)      {
    EventGroup_Send(mySystem.axis[0].evt_act, EVT_ACT_WORK_DISABLE);
    EventGroup_Send(mySystem.axis[1].evt_act, EVT_ACT_WORK_DISABLE);
	
    EventGroup_Send(mySystem.axis[0].evt_can, EVT_CAN_WORK_DISABLE);
    EventGroup_Send(mySystem.axis[1].evt_can, EVT_CAN_WORK_DISABLE);
	
	//还有灯失能
	led_set_event(0, EVT_LEDx_DISABLE);
	led_set_event(1, EVT_LEDx_DISABLE);
}
static void sys_enter_fault(void)     {}

/**
 * @brief  进入急停状态：所有推杆制动
 */
static void sys_enter_emergency(void)
{
    for (int i = 0; i < MAX_AXIS_NUM; i++) {
        EventGroup_Send(mySystem.axis[i].evt_act, EVT_ACT_HOLD);
    }
}



