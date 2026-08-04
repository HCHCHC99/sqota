// #ifndef __LOOP_CONFIG_H
// #define __LOOP_CONFIG_H

// #include <stdint.h>
// #include "state_engine.h"

// // 4.1 位置环相关数据结构（防过冲专用）
// // 位置环配置参数（可存储，默认值内置）
// typedef struct {
//     int32_t target_pos_mm;          // 目标位置（mm）
//     int32_t pos_dead_zone_mm;       // 位置到位死区（mm），防止频繁启停
//     int32_t decelerate_dist_mm;     // 预减速距离（mm），防过冲核心
//     int32_t pos_error_threshold;    // 位置超差阈值（mm），超差触发保护
//     uint16_t pos_loop_period;       // 位置环周期（ms），默认10ms
//     float pos_p_gain;               // 位置环P增益（防过冲，小增益收敛）
//     float pos_i_gain;               // 位置环I增益（消除静差）
//     uint8_t enable_decelerate;      // 使能预减速（防过冲开关）
// } PosLoopConfig_t;

// // 位置环实时数据
// typedef struct {
//     int32_t current_pos_mm;         // 当前位置（来自位置传感器）
//     int32_t pos_error;              // 当前位置偏差（目标-当前）
//     float pos_output;               // 位置环输出（最大允许电流/出力）
//     uint8_t pos_reached;            // 位置到位标志（1=到位，0=未到位）
//     uint8_t pos_over_error;         // 位置超差标志（1=超差，0=正常）
//     uint8_t in_decelerate;          // 处于减速态标志（1=减速，0=正常）
// } PosLoopData_t;

// // 4.2 电流环相关数据结构（限流+堵转专用）
// // 电流环配置参数（可存储，默认值内置）
// typedef struct {
//     uint16_t max_current_mA;        // 最大允许电流阈值（mA）
//     uint16_t current_hysteresis;    // 电流滞回阈值（mA）
//     uint16_t current_loop_period;   // 电流环周期（ms）默认5ms
//     float cur_p_gain;               // P增益
//     float cur_i_gain;               // I增益
//     uint8_t pwm_max;                // PWM最大占空比0~100
//     uint8_t pwm_min;                // PWM最小占空比0~100
//     uint8_t enable_current_loop;     // 总使能
//     float i_limit_max;              // 积分上限
//     float i_limit_min;              // 积分下限
//     uint16_t block_current_thr;     // 堵转判定最小电流
//     uint8_t pwm_slew_rate;          // PWM斜率限制%/ms
//     uint8_t reserve[4];             // 预留
// } CurLoopConfig_t ;

// // 电流环实时数据
// typedef struct {
//     uint16_t current_mA;            // 当前电流mA
//     uint16_t target_current_mA;     // 目标电流mA
//     int16_t  current_error;         // 偏差
//     uint8_t  pwm_duty;              // 输出PWM
//     uint8_t  in_current_limit;      // 限流状态
//     // 堵转
//     uint8_t  block_detect_en;       // 堵转检测使能
//     uint32_t block_duration_ms;     // 判定时间
//     uint32_t block_timer;           // 堵转计时器
//     int32_t  min_pos_change;        // 最小位置变化量
//     uint8_t  blocked_flag;          // 堵转标志
//     // 补充项
//     float    integral;              // 积分（增量PI用）
//     uint8_t  last_pwm_duty;         // 上一周期PWM
//     uint8_t  current_over_limit;    // 超限标志
//     int32_t  block_pos_ref;         // 堵转位置基准
//     uint8_t  debug_flag;            // 调试标记
// } CurLoopData_t ;

// // 4.3 推杆执行器全局数据（跨模块共享，通过消息传递）
// typedef struct {
//     SysState_t sys_state;           // 系统当前状态
//     ActuatorState_t act_state;      // 推杆当前状态
//     MotorState_t motor_state;       // 电机当前状态
//     PosLoopData_t pos_loop;         // 位置环实时数据
//     CurLoopData_t cur_loop;         // 电流环实时数据
//     int32_t sys_target_pos;         // 系统下发的目标位置（mm）
//     uint8_t fault_code;             // 故障码（0=无故障）
// } ActuatorGlobalData_t;

// // 全局数据外部声明（供各模块访问）
// extern ActuatorGlobalData_t g_act_global_data;

// #endif // __LOOP_CONFIG_H

