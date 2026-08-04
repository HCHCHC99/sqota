/**
 * @file    can_protocol.h
 * @brief   CAN 协议层 — CAN ID 数据结构和报文解析
 * @note    定义所有已知 CAN ID 的位域结构体、收发联合体、业务枚举。
 *          依赖 can_module.h (硬件类型)，被 app_can.h (应用层) 引用。
 *
 * 层次关系:
 *   can_module.h  (HAL: 硬件配置 + 收发原语)
 *        ↑
 *   can_protocol.h (协议层: 本文件)
 *        ↑
 *   app_can.h      (应用层: 电机状态机 + 消息发布)
 */

#ifndef __CAN_PROTOCOL_H__
#define __CAN_PROTOCOL_H__

#include "can_module.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*==============================================================================
 * 协议层枚举 — 对应 CAN 报文中的位域
 *============================================================================*/

/** 电机指令 */
typedef enum {
    CAN_CMD_IDLE = 0,       /* 默认/未启动 */
    CAN_CMD_MANU_COMB,      /* 手动结合 */
    CAN_CMD_MANU_SEP,       /* 手动分离 */
    CAN_CMD_AUTO_COMB,      /* 自动结合 */
    CAN_CMD_AUTO_SEP        /* 自动分离 */
} can_cmd_t;

/** 档位状态 */
typedef enum {
    GEAR_INVALID = 0,       /* 无效/未知 */
    GEAR_1 = 1,             /* 1档 (001) */
    GEAR_2 = 2,             /* 2档 (010) */
    GEAR_3 = 3,             /* 3档 (011) */
    GEAR_NEUTRAL = 4,       /* 空档 (100) */
    GEAR_4 = 5              /* 4档 (101) */
} gear_status_t;

/** 是否支持一键标定 */
typedef enum {
    CALIB_NOT_SUPPORT = 0,
    CALIB_SUPPORT = 1
} calib_support_t;

/** 温度标定使能 */
typedef enum {
    AWD_CAL_DISABLE = 0,
    AWD_CAL_ENABLE = 1
} awd_cal_enable_t;

/** 机型特殊标志 */
typedef enum {
    SPC_DISABLE = 0,
    SPC_ENABLE = 1
} machine_type_spc_t;

/** 标定状态 */
typedef enum {
    CAL_DISABLE = 0,
    CAL_ENABLE = 1
} cal_stat_t;

/** 运行状态 */
typedef enum {
    RUN_STAT_IDLE = 0,          /* 默认/空闲 */
    RUN_STAT_AUTO_COMB  = 1,    /* 自动结合中 */
    RUN_STAT_AUTO_SEP   = 2,    /* 自动分离中 */
    RUN_STAT_MANU_COMB  = 3,    /* 手动结合中 */
    RUN_STAT_MANU_SEP   = 4,    /* 手动分离中 */
    RUN_STAT_COMB_DONE  = 5,    /* 已结合 */
    RUN_STAT_SEP_DONE   = 6,    /* 已分离 */
    RUN_STAT_ERR_COMB   = 7,    /* 结合失败 */
    RUN_STAT_ERR_SEP    = 8,    /* 分离失败 */
    RUN_STAT_ERR_SPD    = 9,    /* 转速>1.5 禁止操作 */
    RUN_STAT_ERR_GEAR_STOP = 10,/* 高档位阻止 */
    RUN_STAT_ERR_COMB_GEAR = 11 /* 结合模式档位异常 */
} run_stat_t;

/** 故障状态 (简化版) */
typedef enum {
    FAULT_STAT_IDLE = 0,
    FAULT_STAT_ERR_COMB    = 1, /* 结合失败 */
    FAULT_STAT_ERR_SEP     = 2, /* 分离失败 */
    FAULT_STAT_ERR_SPD     = 3, /* 转速过高 */
    FAULT_STAT_ERR_GEAR_STOP = 4,/* 高档位阻止 */
    FAULT_STAT_ERR_COMB_GEAR = 5 /* 结合模式档位异常 */
} fault_stat_t;

/*==============================================================================
 * 辅助宏
 *============================================================================*/

/** 从指定 CAN ID 数据中提取速度值 */
#define GET_SPEED_FROM_FDF0(data)   (((uint16_t)((data).id_FDF0.speed_high) << 8) | (data).id_FDF0.speed_low) * 0.1f
#define GET_SPEED_FROM_31F9(data)   (((uint16_t)((data).id_31F9.speed_high) << 8) | (data).id_31F9.speed_low) * 0.1f

/*==============================================================================
 * 接收 CAN ID 数据结构
 *============================================================================*/

/** ID: 18FF9DF0 / 18FFC060 (电机控制及状态) */
typedef struct {
    struct {
        can_cmd_t cmd_m1 : 4;   /* 电机1指令 (byte0 bit0-3) */
        can_cmd_t cmd_m2 : 4;   /* 电机2指令 (byte0 bit4-7) */
    } byte0;

    struct {
        uint8_t highest_gear_signal : 1;  /* 最高档信号 */
        uint8_t resv0               : 7;
    } byte1;

    struct {
        machine_type_spc_t en_gk_only          : 2;
        calib_support_t    support_calibration : 2;
        uint8_t            resv2               : 4;
    } byte2;

    uint8_t  byte3[5];
} can_18FF9DF0_t;

/** ID: 18FFEC18 (温度标定使能) */
typedef struct {
    uint8_t byte0;
    struct {
        awd_cal_enable_t awd_cal_enable : 1;
        uint8_t          resv0          : 7;
    };
    uint8_t byte2[6];
} can_18FFEC18_t;

/** ID: 18FFFDF0 (车速, 通用) */
typedef struct {
    uint8_t byte0[4];
    uint8_t speed_low;      /* 时速低字节 (0.1km/h) */
    uint8_t speed_high;     /* 时速高字节 (0.1km/h) */
    uint8_t byte6[2];
} can_18FFFDF0_t;

/** ID: 18FF31F9 (车速, 盛硕) */
typedef struct {
    uint8_t speed_low;
    uint8_t speed_high;
    uint8_t byte2[6];
} can_18FF31F9_t;

/** ID: 18FF17F0 (档位状态) */
typedef struct {
    uint8_t byte0[2];
    struct {
        gear_status_t gear_status : 3;
        uint8_t       resv0       : 5;
    } byte2;
    uint8_t byte3[5];
} can_18FF17F0_t;

/** ID: 18FF8818 / 18FFFFF0 (机型选择) */
typedef struct {
    uint8_t byte0[2];
    uint8_t machine_type;
    struct {
        gear_status_t gear_status : 3;
        uint8_t       resv0       : 5;
    };
    struct {
        machine_type_spc_t ce4p_spc : 1;
        uint8_t            resv1    : 7;
    } byte3;
    uint8_t byte4[4];
} can_18FF8818_t;

/** ID: 18FF8018 (是否支持一键标定) */
typedef struct {
    uint8_t byte0[2];
    struct {
        uint8_t          resv0               : 2;
        calib_support_t  support_calibration : 2;
        uint8_t          resv1               : 4;
    };
    uint8_t byte3[5];
} can_18FF8018_t;

/*==============================================================================
 * 发送 CAN ID 数据结构
 *============================================================================*/

/** ID: 18FF0108 / 18FF0109 (电机1/2 状态上报) */
typedef struct {
    struct {
        cal_stat_t cal_stat : 2;
        run_stat_t run_stat : 4;
        uint8_t    resv0    : 2;
    } byte0;
    uint8_t      sensor_vol_l;   /* 传感器电压 低字节 */
    uint8_t      sensor_vol_h;   /* 传感器电压 高字节 */
    uint8_t      current_l;      /* 电流 低字节 */
    uint8_t      current_h;      /* 电流 高字节 */
    uint8_t      resv1[2];
    fault_stat_t fault_stat;
} can_18FF0108_t;

/** ID: 18FF0104 / 18FF0105 (电机1/2 状态上报, 变体) */
typedef struct {
    struct {
        cal_stat_t cal_stat : 2;
        run_stat_t run_stat : 4;
        uint8_t    resv0    : 2;
    } byte0;
    uint8_t      resv1[2];
    uint8_t      current_l;
    uint8_t      current_h;
    uint8_t      sensor_vol_l;
    uint8_t      sensor_vol_h;
    fault_stat_t fault_stat;
} can_18FF0104_t;

/*==============================================================================
 * 收发联合体 — 按 CAN ID 解释 8 字节数据
 *============================================================================*/

/** 接收数据联合体 */
typedef union {
    uint8_t          rx_data[8];
    can_18FF9DF0_t   id_9DF0;       /* 18FF9DF0 / 18FFC060 */
    can_18FFEC18_t   id_EC18;       /* 18FFEC18 */
    can_18FFFDF0_t   id_FDF0;       /* 18FFFDF0 */
    can_18FF31F9_t   id_31F9;       /* 18FF31F9 */
    can_18FF17F0_t   id_17F0;       /* 18FF17F0 */
    can_18FF8818_t   id_8818;       /* 18FF8818 / 18FFFFF0 */
    can_18FF8018_t   id_8018;       /* 18FF8018 */
} can_rx_data_t;

/** 发送数据联合体 */
typedef union {
    uint8_t          tx_data[8];
    can_18FF0108_t   id_0108;       /* 18FF0108 / 18FF0109 */
    can_18FF0104_t   id_0104;       /* 18FF0104 / 18FF0105 */
} can_tx_data_t;

/** CAN 接收消息 (完整一帧) */
typedef struct {
    uint32_t       can_id;
    can_rx_data_t  data;
    uint8_t        len;
} can_rx_message_t;

#ifdef __cplusplus
}
#endif

#endif /* __CAN_PROTOCOL_H__ */
