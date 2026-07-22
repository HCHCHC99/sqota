/**
 * @file    app_can.h
 * @brief   CAN 应用层 — 电机状态管理和消息收发调度
 * @note    依赖 can_protocol.h (协议定义) 和 can_module.h (硬件抽象)。
 *          负责: 接收 CAN 报文 → 解析 → 发布消息; 采集电机状态 → 组帧 → 发送。
 *
 * 层次关系:
 *   can_module.h    (HAL: 硬件配置 + 收发原语)
 *        ↑
 *   can_protocol.h  (协议层: CAN ID 结构体 + 枚举)
 *        ↑
 *   app_can.h        (应用层: 本文件)
 */

#ifndef __APP_CAN_H__
#define __APP_CAN_H__

#include "can_protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

/*==============================================================================
 * 应用层数据结构
 *============================================================================*/

/** 电机状态 (业务聚合) */
typedef struct {
    cal_stat_t   cal_stat;           /* 标定状态 */
    run_stat_t   run_stat;           /* 运行状态 */
    uint16_t     sensor_voltage;     /* 传感器电压 */
    uint16_t     current;            /* 电流 */
    fault_stat_t fault_stat;         /* 故障状态 */
} motor_state_t;

/** CAN 消息 (发布到其他模块) */
typedef struct {
    can_cmd_t          motor1_cmd;
    can_cmd_t          motor2_cmd;
    uint8_t            highest_gear_signal;
    machine_type_spc_t en_gk_only;
    calib_support_t    support_calibration;
    awd_cal_enable_t   awd_cal_enable;
    uint16_t           speed_general;     /* 通用车速 (0.1km/h) */
    uint16_t           speed_shengshuo;   /* 盛硕车速 (0.1km/h) */
    gear_status_t      gear_status;
    uint8_t            machine_type;
    machine_type_spc_t ce4p_spc;
} can_msg_t;

/*==============================================================================
 * 公共 API
 *============================================================================*/

/** CAN 应用层初始化 (调用 can_module_init + 注册回调) */
void app_can_init(void);

/** CAN 应用层主循环任务 (接收分发 + 定时发送) */
void app_can_task(void);

/** CAN 中断回调 (由 HAL 层在 ISR 中调用) */
void app_can_int_callback(void);

/** 设置电机发送使能 (0=禁止, 1=使能) */
void app_can_set_tx_mgr(uint8_t motor_idx, uint8_t enable);

/** 清除温度标定标志 */
void can_clr_awd_cal(void);

/** 清除指定电机指令 */
void can_clr_cmd(uint8_t motor_idx);

#ifdef __cplusplus
}
#endif

#endif /* __APP_CAN_H__ */
