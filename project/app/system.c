/**
 * @file    system.c
 * @brief   控制器顶层系统实现
 */
#include "system.h"
#include "sys_state.h"
#include "act_state.h"
#include "mot_state.h"
#include "axis_typedef.h"
#include "msg_pubsub.h"
#include "msg_topics.h"
#include <stddef.h>
#include "sys_tick.h"

#include "power_module.h"
#include "app_can.h"
#include "can_module.h"
#include "key.h"
#include "can_state.h"
#include "ring_buffer.h"


// 获取数组元素个数
#define ARRAY_ELEM_COUNT(arr)      (sizeof(arr) / sizeof((arr)[0]))
// 获取数组单元素字节大小
#define ARRAY_ELEM_SIZE(arr)       (sizeof((arr)[0]))


/* 全局系统对象 */
System_t mySystem;

// 环形缓冲区数组
#define RING_BUFFER_SIZE            (50)
float axis_1_speed_buff[RING_BUFFER_SIZE];     // 速度采样 环形缓冲区
float axis_2_speed_buff[RING_BUFFER_SIZE];     // 速度采样 环形缓冲区

const char *event_group_system_names[][2] = {
    {"system_event","system_event_"},
    {"event_act_1","event_act_2"},
    {"event_mot_1","event_mot_2"},
    {"event_can_1","event_can_2"},
};

typedef enum {
	S_POT_NORMAL = 0,
	S_POS_ERR = 1,
} S_pot_stat_t;

typedef struct {
    char                name[4];
    uint8_t             idx;
    float               cur_pos;        // 当前位置（绝对位置）
    S_pot_stat_t        status_pot;    // 传感器状态
} S_pos_upd_t;

/**
 * @brief 系统状态 - 位置信息回调处理函数
 *
 * @param topic  消息主题ID（用于区分不同类型的消息）
 * @param data   指向位置信息数据缓冲区的指针
 * @param len    数据缓冲区长度（单位：字节）
 * @param prio   消息优先级
 *
 * @note 该函数用于接收并处理系统发布的位置状态数据
 */
void System_Pos_Callback(uint32_t topic, const void* data, uint16_t len, uint8_t prio){
    // 安全校验：数据长度必须匹配结构体长度
    if (data == NULL || len != sizeof(S_pos_upd_t)) {
        return; // 数据无效直接退出
    }

    // 把 data 转换成结构体指针
    const S_pos_upd_t* p_data = (const S_pos_upd_t*)data;

    if (p_data->idx < MAX_AXIS_NUM  &&  p_data->status_pot == S_POT_NORMAL )      
    {
        
        // 缓冲区满，计算一次推杆速度
        if ( mySystem.axis[p_data->idx].speed_data->write(mySystem.axis[p_data->idx].speed_data,&p_data->cur_pos) == RING_BUF_ERR_FULL )
        {
            uint32_t now = SysTick_GetTick();

            // 计算 当前位置，原始 adc采样电压
            float floatSum = mySystem.axis[p_data->idx].speed_data->getSumFloat(mySystem.axis[p_data->idx].speed_data);
            mySystem.axis[p_data->idx].pos_current = floatSum / RING_BUFFER_SIZE ;

            mySystem.axis[p_data->idx].pos_ctrl.fdb_last = mySystem.axis[p_data->idx].pos_ctrl.fdb;
            mySystem.axis[p_data->idx].pos_ctrl.fdb_last_time = mySystem.axis[p_data->idx].pos_ctrl.fdb_time;

            uint16_t temp = (uint16_t)((mySystem.axis[p_data->idx].pos_current * 25.0f)*10.0f);   // 当前位置  单位：mm，最大值 约50.0000*10
            mySystem.axis[p_data->idx].pos_current = (float)temp / 10.0f;                         // 保留 小数点后一位

            mySystem.axis[p_data->idx].pos_ctrl.fdb = mySystem.axis[p_data->idx].pos_current;
            mySystem.axis[p_data->idx].pos_ctrl.fdb_time = now;

            if (mySystem.axis[p_data->idx].pos_ctrl.fdb < mySystem.axis[p_data->idx].pos_combine + mySystem.axis[p_data->idx].pos_ctrl.stall_thr &&
                mySystem.axis[p_data->idx].pos_ctrl.fdb > mySystem.axis[p_data->idx].pos_combine - mySystem.axis[p_data->idx].pos_ctrl.stall_thr )
            {
                mySystem.axis[p_data->idx].position = ACT_POS_COMBINE;
            }else if (mySystem.axis[p_data->idx].pos_ctrl.fdb < mySystem.axis[p_data->idx].pos_separate + mySystem.axis[p_data->idx].pos_ctrl.stall_thr &&
                    mySystem.axis[p_data->idx].pos_ctrl.fdb > mySystem.axis[p_data->idx].pos_separate - mySystem.axis[p_data->idx].pos_ctrl.stall_thr )
            {
                mySystem.axis[p_data->idx].position = ACT_POS_SEPARATE;
            }else{
                mySystem.axis[p_data->idx].position = ACT_POS_MIDDLE;
            }

            // 速度单位：mm/s
            // 计算当前速度
            mySystem.axis[p_data->idx].speed.speed_value = (mySystem.axis[p_data->idx].pos_current - mySystem.axis[p_data->idx].speed.pos) / (now-mySystem.axis[p_data->idx].speed.timeout) ;
            mySystem.axis[p_data->idx].speed.pos     = mySystem.axis[p_data->idx].pos_current;
            mySystem.axis[p_data->idx].speed.timeout = now;

            // 判定速度是否 为负值
            if (mySystem.axis[p_data->idx].speed.speed_value < 0 )
            {
                mySystem.axis[p_data->idx].speed.speed_value *= -1;
            }

            // 判定速度是否超范围  核算 推杆速  通过计算 推杆 全速约 6mm/s,设定阈值 ，超阈值 丢弃
            if (mySystem.axis[p_data->idx].speed.speed_value < mySystem.axis[p_data->idx].speed.speed_limit)
            {
                // 保留 速度小数点后 1 位
                mySystem.axis[p_data->idx].speed.speed_value = (float)((uint16_t)(mySystem.axis[p_data->idx].speed.speed_value * 10 ))/ 10.0f ;
                mySystem.axis[p_data->idx].spd_ctrl.fdb      = mySystem.axis[p_data->idx].speed.speed_value;

            }
						mySystem.axis[p_data->idx].speed_data->clear(mySystem.axis[p_data->idx].speed_data);
        }

    }
}

/**
 * @brief 系统状态 - 电源电压  回调处理函数
 *
 * @param topic  消息主题ID（用于区分不同类型的消息）
 * @param data   指向位置信息数据缓冲区的指针
 * @param len    数据缓冲区长度（单位：字节）
 * @param prio   消息优先级
 *
 * @note 该函数用于接收并处理系统发布的电源状态数据
 */
void System_Power_Callback(uint32_t topic, const void* data, uint16_t len, uint8_t prio){
    // 安全校验：数据长度必须匹配结构体长度
    if (data == NULL || len != sizeof(power_upd_t)) {
        return; // 数据无效直接退出
    }

	//如果系统不处于工作状态，直接返回
	if (mySystem.sys_sm.cur_state == SYS_STATE_INIT)
	{
		return;
    }

    // 把 data 转换成结构体指针
    const power_upd_t* p_data = (const power_upd_t*)data;

    mySystem.voltage = p_data->voltage;

    // 目前只处理 电源电压状态
    if (p_data->idx == 0 ){
		if (false == mySystem.acc_enable)
		{
			EventGroup_Send(mySystem.sys_evt, EVT_SYS_FAULT);
		}else
		{
            if (p_data->status_volt == POWER_STATUS_OVER_VOLT){            
                EventGroup_Send(mySystem.sys_evt,EVT_SYS_VOLT_OVER);
            }else if (p_data->status_volt == POWER_STATUS_UNDER_VOLT){
				EventGroup_Send(mySystem.sys_evt, EVT_SYS_VOLT_UNDER);
			}else
			{
				EventGroup_Send(mySystem.sys_evt, EVT_SYS_RECOVERY | EVT_SYS_CMD_WORK_ENABLE);//系统故障恢复
			}
		}
	}
	
	//更新电流
	if (p_data->idx == 1 )
	{
		can_write_current(0, p_data->current);
	}
	
	//更新电流
	if (p_data->idx == 2 )
	{
		can_write_current(1, p_data->current);
    }
}

/**
 * @brief CAN 消息接收 
 *
 * @param topic  消息主题ID（用于区分不同类型的消息）
 * @param data   指向位置信息数据缓冲区的指针
 * @param len    数据缓冲区长度（单位：字节）
 * @param prio   消息优先级
 *
 * @note 该函数用于接收并处理系统发布的 can数据
 */
void System_CAN_Callback(uint32_t topic, const void* data, uint16_t len, uint8_t prio){

// 安全校验：数据长度必须匹配结构体长度
    if (data == NULL || len != sizeof(can_msg_t)) {
        return; // 数据无效直接退出
    }
	//如果系统不处于工作状态，直接返回
	if (mySystem.sys_sm.cur_state != SYS_STATE_RUN)
	{
		return;
	}    
    // 把 data 转换成结构体指针
    const can_msg_t* p_data = (const can_msg_t*)data;

	//手动标定
			
	//报文标定 -- 不支持一键自动标定
	if (AWD_CAL_ENABLE == p_data->awd_cal_enable)
	{
		can_clr_awd_cal();
		if (CALIB_NOT_SUPPORT == p_data->support_calibration)
		{
			EventGroup_Send(mySystem.axis[0].evt_can, EVT_CAN_INTO_AUTO_CALIB);
			EventGroup_Send(mySystem.axis[1].evt_can, EVT_CAN_INTO_AUTO_CALIB);
		}
		else if (CALIB_SUPPORT == p_data->support_calibration)
		{
			mySystem.axis[0].is_calib = true;
			mySystem.axis[1].is_calib = true;
			EventGroup_Send(mySystem.axis[0].evt_can, EVT_CAN_CALIB_SUCCESS);
			EventGroup_Send(mySystem.axis[1].evt_can, EVT_CAN_CALIB_SUCCESS);
		}
		
	}
	
		
	//最高档信号
	if (true == p_data->highest_gear_signal || GEAR_4 == p_data->gear_status)
	{
		mySystem.can_over_gear_enable = true;
		
		//如果正在结合
		if (CAN_STATE_COMB_ING == mySystem.axis[0].sm_can.cur_state)
		{
            EventGroup_Send(mySystem.axis[0].evt_act, EVT_ACT_WORK_DISABLE);
            EventGroup_Send(mySystem.axis[0].evt_can, EVT_CAN_OVER_GEAR_COMB);
		}
		//如果已结合
		else if (CAN_STATE_COMB_DONE == mySystem.axis[0].sm_can.cur_state)
		{
            EventGroup_Send(mySystem.axis[0].evt_can, EVT_CAN_COMB_OVER_GEAR);
		}
		
		//如果正在结合
		if (CAN_STATE_COMB_ING == mySystem.axis[1].sm_can.cur_state)
		{
            EventGroup_Send(mySystem.axis[1].evt_act, EVT_ACT_WORK_DISABLE);
            EventGroup_Send(mySystem.axis[1].evt_can, EVT_CAN_OVER_GEAR_COMB);  
		}
		else if (CAN_STATE_COMB_DONE == mySystem.axis[1].sm_can.cur_state)
		{
            EventGroup_Send(mySystem.axis[1].evt_can, EVT_CAN_COMB_OVER_GEAR);
		}                        
	}
	else//高档位消失
	{         
		mySystem.can_over_gear_enable = false;
		if (EVT_CAN_COMB_OVER_GEAR == mySystem.axis[0].sm_can.cur_state)
		{
            EventGroup_Send(mySystem.axis[0].evt_can, EVT_CAN_COMB_NORMAL_GEAR);
		}
		if (EVT_CAN_COMB_OVER_GEAR == mySystem.axis[1].sm_can.cur_state)
        {
            EventGroup_Send(mySystem.axis[1].evt_can, EVT_CAN_COMB_NORMAL_GEAR);
		}
			
	}

    //超速
    if (p_data->speed_general > 15 || p_data->speed_shengshuo > 15)
	{
		mySystem.over_spd_enable = true;
		if (CAN_STATE_COMB_ING == mySystem.axis[0].sm_can.cur_state || CAN_STATE_SEP_ING == mySystem.axis[0].sm_can.cur_state)//正在结合或者正在分离呢
		{
			EventGroup_Send(mySystem.axis[0].evt_act, EVT_ACT_WORK_DISABLE);
			EventGroup_Send(mySystem.axis[0].evt_can, EVT_CAN_OVER_SPD);
			
		}
		
		if (CAN_STATE_COMB_ING == mySystem.axis[1].sm_can.cur_state || CAN_STATE_SEP_ING == mySystem.axis[1].sm_can.cur_state)
		{
			EventGroup_Send(mySystem.axis[1].evt_act, EVT_ACT_WORK_DISABLE);
			EventGroup_Send(mySystem.axis[1].evt_can, EVT_CAN_OVER_SPD);
		}

	}
	else
	{
		mySystem.over_spd_enable = false;
	}
	
	
	if (false == mySystem.axis[0].is_calib)	return;

	
	//结合 -- M1
	if (CAN_CMD_AUTO_COMB == p_data->motor1_cmd)
	{
		can_clr_cmd(0);
		 //超速
		if (true == mySystem.over_spd_enable)
                {
                    EventGroup_Send(mySystem.axis[0].evt_can, EVT_CAN_OVER_SPD);//超速不能结合
                }
		else if (true == mySystem.can_over_gear_enable || true == mySystem.key_over_gear_enable)
                {
                    EventGroup_Send(mySystem.axis[0].evt_can, EVT_CAN_OVER_GEAR_COMB);//高档位，不能结合
                }
                else
                {
                    EventGroup_Send(mySystem.axis[0].evt_act, EVT_ACT_COMBINE);//可以结合--发给推杆状态机
                    EventGroup_Send(mySystem.axis[0].evt_can, EVT_CAN_COMB);//可以结合
                }
        }
        //分离
        else if(CAN_CMD_AUTO_SEP == p_data->motor1_cmd)
        {
		can_clr_cmd(0);
                 //超速
		if (true == mySystem.over_spd_enable)
                {
                    EventGroup_Send(mySystem.axis[0].evt_can, EVT_CAN_OVER_SPD);//超速不能分离
                }
                else
                {
                    EventGroup_Send(mySystem.axis[0].evt_act, EVT_ACT_SEPARATE);//可以分离--发给推杆状态机
                    EventGroup_Send(mySystem.axis[0].evt_can, EVT_CAN_SEP);//可以分离
                }
        }
	
	
	if (false == mySystem.axis[1].is_calib)	return;
        
        //结合
        if (CAN_CMD_AUTO_COMB == p_data->motor2_cmd)
        {
		can_clr_cmd(1);
                 //超速
		if (true == mySystem.over_spd_enable)
                {
                    EventGroup_Send(mySystem.axis[1].evt_can, EVT_CAN_OVER_SPD);//超速不能结合
                }
		else if (true == mySystem.can_over_gear_enable || true == mySystem.key_over_gear_enable)
                {
                    EventGroup_Send(mySystem.axis[1].evt_can, EVT_CAN_OVER_GEAR_COMB);//高档位，不能结合
                }
                else
                {
                    EventGroup_Send(mySystem.axis[1].evt_act, EVT_ACT_COMBINE);//可以结合--发给推杆状态机
                    EventGroup_Send(mySystem.axis[1].evt_can, EVT_CAN_COMB);//可以结合
                }
        }
        //分离
        else if(CAN_CMD_AUTO_SEP == p_data->motor2_cmd)
        {
		can_clr_cmd(1);
                 //超速
		if (true == mySystem.over_spd_enable)
                {
                    EventGroup_Send(mySystem.axis[1].evt_can, EVT_CAN_OVER_SPD);//超速不能分离
                }
                else
                {
                    EventGroup_Send(mySystem.axis[1].evt_act, EVT_ACT_SEPARATE);//可以分离--发给推杆状态机
                    EventGroup_Send(mySystem.axis[1].evt_can, EVT_CAN_SEP);//可以分离
                }
        }
        

}


/**
 * @brief key 状态识别 
 *
 * @param topic  消息主题ID（用于区分不同类型的消息）
 * @param data   指向位置信息数据缓冲区的指针
 * @param len    数据缓冲区长度（单位：字节）
 * @param prio   消息优先级
 *
 * @note 该函数用于接收并处理系统发布的 key 状态
 */
void System_Key_Callback(uint32_t topic, const void* data, uint16_t len, uint8_t prio){
    // 安全校验：数据长度必须匹配结构体长度
    if (data == NULL || len != sizeof(Key_Report_t)) {
        return; // 数据无效直接退出
    }

	//如果系统不处于工作状态，直接返回
	if (mySystem.sys_sm.cur_state == SYS_STATE_INIT)
	{
		return;
	}
    // 把 data 转换成结构体指针
    const Key_Report_t* p_data = (const Key_Report_t*)data;
	
    // ACC
	if (0x02 == p_data->key_code)
	{
		//acc标志
		if (KEY_EVENT_LONG == p_data->event)
		{
			mySystem.acc_enable = true;
		}else
		{
			mySystem.acc_enable = false;
		}
		
	}
	
	//高档位
	if (0x05 == p_data->key_code)
	{
		if (KEY_EVENT_LONG == p_data->event)
		{
			mySystem.key_over_gear_enable = true;
                //如果正在结合
                if (CAN_STATE_COMB_ING == mySystem.axis[0].sm_can.cur_state)
                {
                    EventGroup_Send(mySystem.axis[0].evt_act, EVT_ACT_WORK_DISABLE);
                    EventGroup_Send(mySystem.axis[0].evt_can, EVT_CAN_OVER_GEAR_COMB);
                }
                //如果已结合
                else if (CAN_STATE_COMB_DONE == mySystem.axis[0].sm_can.cur_state)
                {
                    EventGroup_Send(mySystem.axis[0].evt_can, EVT_CAN_COMB_OVER_GEAR);
                }
                
                //如果正在结合
                if (CAN_STATE_COMB_ING == mySystem.axis[1].sm_can.cur_state)
                {
                    EventGroup_Send(mySystem.axis[1].evt_act, EVT_ACT_WORK_DISABLE);
                    EventGroup_Send(mySystem.axis[1].evt_can, EVT_CAN_OVER_GEAR_COMB);
                }
                else if (CAN_STATE_COMB_DONE == mySystem.axis[1].sm_can.cur_state)
                {
                    EventGroup_Send(mySystem.axis[1].evt_can, EVT_CAN_COMB_OVER_GEAR);
                }                        
		}else
        {                
			mySystem.key_over_gear_enable = false;
			
                if (EVT_CAN_COMB_OVER_GEAR == mySystem.axis[0].sm_can.cur_state)
                {
                    EventGroup_Send(mySystem.axis[0].evt_can, EVT_CAN_COMB_NORMAL_GEAR);
                }
                if (EVT_CAN_COMB_OVER_GEAR == mySystem.axis[1].sm_can.cur_state)
                {
                    EventGroup_Send(mySystem.axis[1].evt_can, EVT_CAN_COMB_NORMAL_GEAR);
                }
		}
		
	}
	
	if (mySystem.sys_sm.cur_state != SYS_STATE_RUN)
	{
		return;
        }
        
//	//标定	
//	if (0x00 == p_data->key_code)
//		
//	if (0x01 == p_data->key_code)
	//结合
	if (0x03 == p_data->key_code)
	{
		if (KEY_EVENT_CLICK == p_data->event)
		{
			//超速
			if (true == mySystem.over_spd_enable)
        {
                EventGroup_Send(mySystem.axis[0].evt_can, EVT_CAN_OVER_SPD);//超速不能结合
                EventGroup_Send(mySystem.axis[1].evt_can, EVT_CAN_OVER_SPD);//超速不能结合
			}
			else if (true == mySystem.can_over_gear_enable || true == mySystem.key_over_gear_enable)
			{
                EventGroup_Send(mySystem.axis[0].evt_can, EVT_CAN_OVER_GEAR_COMB);//高档位，不能结合
                EventGroup_Send(mySystem.axis[1].evt_can, EVT_CAN_OVER_GEAR_COMB);//高档位，不能结合
			}
			else
			{
				if (true == mySystem.axis[0].is_calib)
				{
					EventGroup_Send(mySystem.axis[0].evt_act, EVT_ACT_COMBINE);//可以结合--发给推杆状态机
					EventGroup_Send(mySystem.axis[0].evt_can, EVT_CAN_COMB);//可以结合		
				}					
				
				if (true == mySystem.axis[1].is_calib)
				{
					EventGroup_Send(mySystem.axis[1].evt_act, EVT_ACT_COMBINE);//可以结合--发给推杆状态机
					EventGroup_Send(mySystem.axis[1].evt_can, EVT_CAN_COMB);//可以结合
				}
			}
		}
	}
	
	//分离
	if (0x04 == p_data->key_code)
	{
		if (KEY_EVENT_CLICK == p_data->event)
		{
			//超速
			if (true == mySystem.over_spd_enable)
			{
                EventGroup_Send(mySystem.axis[0].evt_can, EVT_CAN_OVER_SPD);//超速不能分离
                EventGroup_Send(mySystem.axis[1].evt_can, EVT_CAN_OVER_SPD);//超速不能分离
			}
			else
			{
				if (true == mySystem.axis[0].is_calib)
				{					
					EventGroup_Send(mySystem.axis[0].evt_act, EVT_ACT_SEPARATE);//可以分离--发给推杆状态机
					EventGroup_Send(mySystem.axis[0].evt_can, EVT_CAN_SEP);//可以分离
					
				}

				if (true == mySystem.axis[1].is_calib)
				{
					EventGroup_Send(mySystem.axis[1].evt_act, EVT_ACT_SEPARATE);//可以分离--发给推杆状态机
					EventGroup_Send(mySystem.axis[1].evt_can, EVT_CAN_SEP);//可以分离
					
				}
				
			}
		}

	}

}


// extern const Mot_HwConfig_t MOT_HW_TABLE[MOT_INDEX_MAX][ARM_INDEX_MAX];

/**
 * @brief  系统初始化
 * @note   状态机、事件组
 */
void State_Init(void)
{
    // 系统状态机 & 事件组初始化
    Sys_State_Init(&mySystem.sys_sm);
    mySystem.sys_evt = EventGroup_Create(event_group_system_names[0][0]);

    // GPIO 初始化
    stc_gpio_init_t stcGpioInit;
    (void)GPIO_StructInit(&stcGpioInit);
    stcGpioInit.u16PinDir = PIN_DIR_OUT;

    RingBuf_CreateInstance("Vaxis_1", axis_1_speed_buff, ARRAY_ELEM_COUNT(axis_1_speed_buff), ARRAY_ELEM_SIZE(axis_1_speed_buff), RING_MODE_DISCARD);
    RingBuf_CreateInstance("Vaxis_2", axis_2_speed_buff, ARRAY_ELEM_COUNT(axis_2_speed_buff), ARRAY_ELEM_SIZE(axis_2_speed_buff), RING_MODE_DISCARD);

    // 推杆 & 电机初始化
    for (int i = 0; i < MAX_AXIS_NUM; i++) {
        Axis_t *ax = &mySystem.axis[i];
        ax->id = i;
        ax->enable = 0;
        ax->stall_count = 0;
        ax->stall_count_thr = 1500;
        ax->success_count = 0;
        ax->success_count_thr = 1500;

        // 测试时，写死，实际应从 标定数据获取
        ax->pos_combine = 45.0;
        ax->pos_separate = 20.0;

        if (i == 0)
        {
            ax->speed_data = RingBuf_GetByName("Vaxis_1");
        }else if (i == 1)
        {
            ax->speed_data = RingBuf_GetByName("Vaxis_2");
        }

        ax->speed.speed_value = 0.0;
        ax->speed.pos = 0.0;
        ax->speed.speed_limit = 8.0;
        ax->speed.timeout_thr = 20;
  
        ax->evt_act = EventGroup_Create(event_group_system_names[1][i]);
        Act_State_Init(&ax->sm_act);

        ax->motor.id = i;
        ax->motor.enable = 0;        
        ax->motor.evt_mot = EventGroup_Create(event_group_system_names[2][i]);        
        Mot_State_Init(&ax->motor.sm_mot);

        Motor_t *mot = &mySystem.axis[i].motor;
        mot->hw = &MOTOR_HW_LIST[i];
        if (mot->hw->hu->active_level) {
            stcGpioInit.u16PullUp = PIN_STAT_RST;
        } else {
            stcGpioInit.u16PullUp = PIN_STAT_SET;
        }
        GPIO_Init(mot->hw->hu->gpio_port, mot->hw->hu->pin, &stcGpioInit);

        if (mot->hw->hv->active_level) {
            stcGpioInit.u16PullUp = PIN_STAT_RST;
        } else {
            stcGpioInit.u16PullUp = PIN_STAT_SET;
        }
        GPIO_Init(mot->hw->hv->gpio_port, mot->hw->hv->pin, &stcGpioInit);
		
		ax->evt_can = EventGroup_Create(event_group_system_names[3][i]);        
        Can_State_Init(&ax->sm_can);
    }

    // 需要 订阅 数据
    // 1.位置信息    
    // 2.速度信息(通过位置信息计算)
    Msg_Subscribe(TOPIC_POS_UPDATED, System_Pos_Callback);
    // 3.电源电压信息
    Msg_Subscribe(TOPIC_PWR_STATE_UPDATED, System_Power_Callback);
    // 4.can指令数据
    Msg_Subscribe(TOPIC_CAN_MSG_RECEIVED, System_CAN_Callback);
    // 5.外部IO数据，抽象成 按键指令数据
    Msg_Subscribe(TOPIC_KEYS_STATE, System_Key_Callback);

    //系统初始化完成
    EventGroup_Send(mySystem.sys_evt, EVT_SYS_INIT_DONE | EVT_SYS_CMD_WORK_ENABLE);
}

/**
 * @brief  系统周期调度任务
 * @note   依次调度系统、推杆、电机状态机
 */
void State_Task(void)
{
    // 调度系统状态 任务
    Sys_State_Task();

    // 调度所有轴状态 任务
    for (int i = 0; i < MAX_AXIS_NUM; i++) {
        Act_State_Task(&mySystem.axis[i]);
        Mot_State_Task(&mySystem.axis[i]);
        Can_State_Task(&mySystem.axis[i]);
    }
}


