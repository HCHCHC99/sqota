
#include "app_can.h"
#include "sys_config.h"
#include "msg_pubsub.h"
#include "msg_topics.h"

#include "can_state.h"

#if SYS_ENABLE_UDS
#include "uds/uds_rx_entry.h"
#include "uds/uds_ota.h"
#include "uds/adapter_can.h"
#endif


//can模块使能
#if SYS_ENABLE_CAN

/* 发送状态管理器 */
typedef struct {
    uint32_t last_cycle;        // 上一个发送周期的起始时间
    uint32_t next_tx_time;      // 下一帧的发送时间点
    uint8_t  step;              // 0:空闲, 1~4:发送第几帧
	
    /* 推杆使能标志 */
    uint8_t motor1_enabled;   // 推杆1使能：1=使能，0=禁用
    uint8_t motor2_enabled;   // 推杆2使能：1=使能，0=禁用
	
} can_tx_manager_t;

static can_tx_manager_t tx_mgr = {0};

/* 外部硬件配置表 */
extern const can_cfg_t CAN_HW;

/* 全局实例（静态，仅本文件可见） */
can_handle_t can_handle;
// 定义两个电机状态实例
motor_state_t g_motor1_state;
motor_state_t g_motor2_state;
//准备发布的
static can_msg_t s_can_msg = {
    .motor1_cmd          = CAN_CMD_IDLE,
    .motor2_cmd          = CAN_CMD_IDLE,
    .highest_gear_signal = 0,
    .en_gk_only          = SPC_DISABLE,                     // 根据 machine_type_spc_t 定义填0
    .support_calibration = CALIB_NOT_SUPPORT,
    .awd_cal_enable      = AWD_CAL_DISABLE,
    .speed_general       = 0,
    .speed_shengshuo     = 0,
    .gear_status         = GEAR_INVALID,
    .machine_type        = 0,
    .ce4p_spc            = SPC_DISABLE,
};

/* ==================== 5. 定时发送函数（由主循环周期性调用） ==================== */
// 发送间隔（毫秒），根据实际需求调整（例如 500ms）
#define CAN_TX_INTERVAL_MS 500



/* ==================== 修改后的数据处理函数 ==================== */
void data_process_9DF0(can_rx_data_t *data, uint8_t len) {
    if (len < 8) return;

    // 更新电机指令
    s_can_msg.motor1_cmd = data->id_9DF0.byte0.cmd_m1;
    s_can_msg.motor2_cmd = data->id_9DF0.byte0.cmd_m2;

    // 更新最高档信号
    s_can_msg.highest_gear_signal = data->id_9DF0.byte1.highest_gear_signal;

    // 更新 en_gk_only 和 support_calibration
    s_can_msg.en_gk_only = data->id_9DF0.byte2.en_gk_only;
    s_can_msg.support_calibration = data->id_9DF0.byte2.support_calibration;

    // 其他字段（如需要可添加）
}

void data_process_8018(can_rx_data_t *data, uint8_t len) {
    if (len < 8) return;
    // 更新 support_calibration（另一来源）
    s_can_msg.support_calibration = data->id_8018.support_calibration;
}

void data_process_EC18(can_rx_data_t *data, uint8_t len) {
    if (len < 8) return;
    s_can_msg.awd_cal_enable = data->id_EC18.awd_cal_enable;
}

void data_process_17F0(can_rx_data_t *data, uint8_t len) {
    if (len < 8) return;
    s_can_msg.gear_status = data->id_17F0.byte2.gear_status;
}

void data_process_FDF0(can_rx_data_t *data, uint8_t len) {
    if (len < 8) return;
    uint16_t speed_raw = ((uint16_t)data->id_FDF0.speed_high << 8) | data->id_FDF0.speed_low;
    s_can_msg.speed_general = speed_raw;  // 存储原始值 (0.1km/h)
}

void data_process_31F9(can_rx_data_t *data, uint8_t len) {
    if (len < 8) return;
    uint16_t speed_raw = ((uint16_t)data->id_31F9.speed_high << 8) | data->id_31F9.speed_low;
    s_can_msg.speed_shengshuo = speed_raw;
}

void data_process_8818(can_rx_data_t *data, uint8_t len) {
    if (len < 8) return;
    s_can_msg.machine_type = data->id_8818.machine_type;
    s_can_msg.ce4p_spc = data->id_8818.byte3.ce4p_spc;
}

//状态发布
static void can_publish_stat(void)
{
	Msg_Publish(TOPIC_CAN_MSG_RECEIVED, &s_can_msg, sizeof(can_msg_t), MSG_PRIO_HIGH);
}


/* ==================== 接收处理主函数（修改后） ==================== */
void app_can_receive(void) {
    stc_can_rx_frame_t rxFrame;
    if (can_read(&can_handle.can_rx, &rxFrame) == CAN_RET_OK) {
        can_rx_message_t rx_mess;
        memset(&rx_mess, 0, sizeof(rx_mess));

        rx_mess.can_id = rxFrame.u32ID;
        rx_mess.len = rxFrame.DLC;
        memcpy(&rx_mess.data, rxFrame.au8Data, rx_mess.len);

        switch (rx_mess.can_id) {
            case 0x18FF9DF0:
            case 0x18FFC060:
                data_process_9DF0(&rx_mess.data, rx_mess.len);
                break;
            case 0x18FF8018:
                data_process_8018(&rx_mess.data, rx_mess.len);
                break;
            case 0x18FFEC18:
                data_process_EC18(&rx_mess.data, rx_mess.len);
                break;
            case 0x18FF17F0:
                data_process_17F0(&rx_mess.data, rx_mess.len);
                break;
            case 0x18FFFDF0:
                data_process_FDF0(&rx_mess.data, rx_mess.len);
                break;
            case 0x18FF31F9:
                data_process_31F9(&rx_mess.data, rx_mess.len);
                break;
            case 0x18FF8818:
            case 0x18FFFFF0:
                data_process_8818(&rx_mess.data, rx_mess.len);
                break;
            default:
#if SYS_ENABLE_UDS
                uds_rx_entry(rx_mess.can_id, rx_mess.data.rx_data, rx_mess.len);
#endif
                return;  // 未知 ID，不发布
        }
        // 每次成功处理后发布最新状态
        can_publish_stat();
    }
}
/**
 * @brief 构建 ID 0x18FF0108 发送数据（电机1）
 * @param data 指向 can_tx_data_t 的指针（用于填充）
 * @param motor 电机状态指针
 */
static void build_can_0108(can_tx_data_t *data, const motor_state_t *motor)
{
    // 填充 byte0
    data->id_0108.byte0.cal_stat = motor->cal_stat;
    data->id_0108.byte0.run_stat = motor->run_stat;
    data->id_0108.byte0.resv0 = 0;

    // 传感器电压（低字节在前）
    data->id_0108.sensor_vol_l = (uint8_t)(motor->sensor_voltage & 0xFF);
    data->id_0108.sensor_vol_h = (uint8_t)(motor->sensor_voltage >> 8);

    // 电流（低字节在前）
    data->id_0108.current_l = (uint8_t)(motor->current & 0xFF);
    data->id_0108.current_h = (uint8_t)(motor->current >> 8);

    // 保留字节
    data->id_0108.resv1[0] = 0;
    data->id_0108.resv1[1] = 0;

    // 故障状态
    data->id_0108.fault_stat = motor->fault_stat;
}


/**
 * @brief 构建 ID 0x18FF0104 发送数据（电机2）
 * @param data 指向 can_tx_data_t 的指针
 * @param motor 电机状态指针
 */
static void build_can_0104(can_tx_data_t *data, const motor_state_t *motor)
{
    // 填充 byte0（与0108相同）
    data->id_0104.byte0.cal_stat = motor->cal_stat;
    data->id_0104.byte0.run_stat = motor->run_stat;
    data->id_0104.byte0.resv0 = 0;

    // 保留2字节
    data->id_0104.resv1[0] = 0;
    data->id_0104.resv1[1] = 0;

    // 电流（低字节在前）
    data->id_0104.current_l = (uint8_t)(motor->current & 0xFF);
    data->id_0104.current_h = (uint8_t)(motor->current >> 8);

    // 传感器电压（低字节在前）
    data->id_0104.sensor_vol_l = (uint8_t)(motor->sensor_voltage & 0xFF);
    data->id_0104.sensor_vol_h = (uint8_t)(motor->sensor_voltage >> 8);

    // 故障状态
    data->id_0104.fault_stat = motor->fault_stat;
}

/* ==================== 4. 获取电机状态信息的接口（预留） ==================== */
/**
 * @brief 更新电机1的状态（从硬件或算法中获取）
 * 此函数应在主循环或定时器中调用，用于刷新 g_motor1_state
 */
static void update_motor1_state(void)
{
    motor_state_t* pstate = can_get_mag_state(0);

    // 临时赋值（演示用）
    g_motor1_state.cal_stat = pstate->cal_stat;
    g_motor1_state.run_stat = pstate->run_stat;
    g_motor1_state.sensor_voltage = pstate->sensor_voltage;   // 例如 25.0V
    g_motor1_state.current = pstate->current;           // 5.0A
    g_motor1_state.fault_stat = pstate->fault_stat;
}

static void update_motor2_state(void)
{
    motor_state_t* pstate = can_get_mag_state(0);
	
    g_motor2_state.cal_stat = pstate->cal_stat;
    g_motor2_state.run_stat = pstate->run_stat;
    g_motor2_state.sensor_voltage = pstate->sensor_voltage;
    g_motor2_state.current = pstate->current;
    g_motor2_state.fault_stat = pstate->fault_stat;
}

void app_can_set_tx_mgr(uint8_t motor_idx, uint8_t enable) //0 - 失能； 1 - 使能
{
	if (0 == motor_idx)
	{
		tx_mgr.motor1_enabled = enable;
	}
	else if (1 == motor_idx)
	{
		tx_mgr.motor2_enabled = enable;
	}
	
}


/* 应用层发送函数，需要被高频调用（例如每1ms或更短） */
void app_can_transmit(void) 
{
    uint32_t now = SysTick_GetTick();

    /* 状态0：空闲，检查是否开启新周期（500ms） */
    if (tx_mgr.step == 0) {
        if (now - tx_mgr.last_cycle >= 500) {
            tx_mgr.last_cycle = now;
            tx_mgr.step = 1;                // 开始发送第一帧
            tx_mgr.next_tx_time = now;      // 立即发送
        } else {
            return;
        }
    }

    /* 等待发送时机 */
    if (now < tx_mgr.next_tx_time) {
        return;
    }

    /* 刷新电机状态（获取最新数据） */
    update_motor1_state();
    update_motor2_state();

    can_tx_data_t tx_data;

    /* 按步骤发送 */
    switch (tx_mgr.step) {
        case 1:
			if (tx_mgr.motor1_enabled)
			{
                build_can_0108(&tx_data, &g_motor1_state);
                can_transmit_ext(0x18FF0108, (uint8_t*)&tx_data, 8);
			}            
            break;
        case 2:
			if (tx_mgr.motor2_enabled)
			{
                build_can_0108(&tx_data, &g_motor2_state);
                can_transmit_ext(0x18FF0109, (uint8_t*)&tx_data, 8);
			} 
            break;
        case 3:
			if (tx_mgr.motor1_enabled)
			{
                build_can_0104(&tx_data, &g_motor1_state);
                can_transmit_ext(0x18FF0104, (uint8_t*)&tx_data, 8);
			}
            break;
        case 4:
			if (tx_mgr.motor2_enabled)
			{
                build_can_0104(&tx_data, &g_motor2_state);
                can_transmit_ext(0x18FF0105, (uint8_t*)&tx_data, 8);
			}
            break;
	        default:
            break;
    }

    /* 移动到下一帧 */
    tx_mgr.step++;
    if (tx_mgr.step > 4) {
        tx_mgr.step = 0;                // 一轮完成，回到空闲
        tx_mgr.next_tx_time = 0;
    } else {
        tx_mgr.next_tx_time = now + 1;  // 间隔1ms（非阻塞）
    }
}

//清除报文标定标志
void can_clr_awd_cal(void)
{
	s_can_msg.awd_cal_enable = AWD_CAL_DISABLE;
}

//清除动作命令
void can_clr_cmd(uint8_t motor_idx)
{
	if (0 == motor_idx)
	{
		s_can_msg.motor1_cmd = CAN_CMD_IDLE;
	}else if (1 == motor_idx)
	{
		s_can_msg.motor2_cmd = CAN_CMD_IDLE;
	}
	
}

	
//初始化
void app_can_init(void)
{
	can_module_init(&can_handle,&CAN_HW);
#if SYS_ENABLE_UDS
	CanIf_Init(&can_handle);
	UdsOta_Init();
#endif
	
	//根据配置信息对报文进行使能
	
}


//周期调度函数
void app_can_task(void)
{
	//can接收
	app_can_receive();
	//can发送
	app_can_transmit();
	//状态发布app_can_receive接收到数据再发布
	can_evt_publish(&s_can_msg);
}

#endif
