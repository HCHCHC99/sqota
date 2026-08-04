#ifndef __CAN_STATE_H
#define __CAN_STATE_H


#include "app_can.h"
#include "axis_typedef.h"

typedef enum {
    CAN_STATE_INIT = 0,			// 初始化状态
    CAN_STATE_IDLE ,			// 空闲状态
   
    CAN_STATE_NO_CALIB,			//默认状态:未标定，
    CAN_STATE_DEFAULT,			//默认状态:已标定，不在结合/分离位置
	
	CAN_STATE_COMB_ING,			//正在结合
	CAN_STATE_SEP_ING,			//正在分离
	
	CAN_STATE_COMB_DONE,		//已结合
	CAN_STATE_SEP_DONE,			//已分离
	
	CAN_STATE_COMB_FAULT,		//结合失败
	CAN_STATE_SEP_FAULT,		//分离失败
	
	CAN_STATE_SPD_FAULT ,		//速度>1.5km/h 禁止结合/分离报警
	CAN_STATE_GEAR_COMB_FAULT ,	//最高挡禁止结合四驱报警
	CAN_STATE_COMB_GEAR_FAULT ,	//四驱模式结合最高档报警
	
	CAN_STATE_MANU_CALIB, 		//手动校准 :系统进入这个手动校准之后传递过来
	CAN_STATE_DIRECT_CALIB, 	//一键校准 ：收到报文后，标记当前位置为标定点
	CAN_STATE_AUTO_CALIB, 		//一键自动校准：收到报文后，进入一键自动标定状态：向分离方向运行到堵转后回退2mm记为分离点
	
	CAN_STATE_DISABLE ,			// 失能
} can_state_t;

//can状态机事件定义
#define EVT_CAN_WORK_ENABLE			(1U << 0)		// CAN 工作使能
#define EVT_CAN_WORK_DISABLE		(1U << 1)		// CAN 工作禁止

#define EVT_CAN_COMB				(1U << 2)		// CAN 结合
#define EVT_CAN_COMB_DONE       	(1U << 4)		// CAN 结合完成
#define EVT_CAN_COMB_FAULT       	(1U << 6)		// CAN 结合失败

#define EVT_CAN_SEP					(1U << 3)		// CAN 分离
#define EVT_CAN_SEP_DONE       		(1U << 5)		// CAN 分离完成 
#define EVT_CAN_SEP_FAULT       	(1U << 7)		// CAN 分离失败

#define EVT_CAN_OVER_SPD			(1U << 8)		// CAN 超速:运行中超速/超速收到命令
#define EVT_CAN_OVER_SPD_5S			(1U << 9)		// CAN 超速:运行中超速/超速收到命令
#define EVT_CAN_OVER_GEAR_COMB		(1U << 10)		// CAN 非结合状态下+高档位+收到结合命令
#define EVT_CAN_OVER_GEAR_COMB_5S	(1U << 11)		// CAN 非结合状态下+高档位+收到结合命令
#define EVT_CAN_COMB_OVER_GEAR		(1U << 12)		// CAN 结合状态下+高档位
#define EVT_CAN_COMB_NORMAL_GEAR	(1U << 13)		// CAN 结合状态下，原来有高档位，报上一个异常，高档位消失了

#define EVT_CAN_INTO_MANU_CALIB		(1U << 14)		// CAN 进入手动校准模式
#define EVT_CAN_EXIT_MANU_CALIB		(1U << 15)		// CAN 退出手动校准模式
#define EVT_CAN_CALIB_SUCCESS		(1U << 16)		// CAN 校准成功
#define EVT_CAN_CALIB_FAIL			(1U << 17)		// CAN 校准失败


#define EVT_CAN_NO_CALIB			(1U << 18)		// CAN 未校准
#define EVT_CAN_NO_COMB_SEP			(1U << 19)		// CAN 不在结合/分离位置

motor_state_t* can_get_mag_state(Mot_Index_t idx);

void Can_State_Init(StateMachine_t *sm);
//can状态机
void Can_State_Task(Axis_t *axis);

//写入电流
void can_write_current(uint8_t motor_idx, uint16_t current);

void can_evt_publish(can_msg_t* can_msg);
#endif

