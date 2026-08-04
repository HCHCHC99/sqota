/**
 * @file power_manager.h
 * @brief 电源管理模块（电压/极性/过流监测）
 */

#ifndef __POWER_MANAGER_H
#define __POWER_MANAGER_H

#include "sys_config.h"
#include <stdint.h>

/**
 * @brief 电源总体状态
 */
typedef enum {
    PWR_STATE_OK               = 0,    /* 电源正常 */
    PWR_STATE_UNDER_VOLTAGE    = 1,    /* 欠压 */
    PWR_STATE_OVER_VOLTAGE     = 2,    /* 过压 */
    PWR_STATE_REVERSE          = 3,    /* 电源反接 */
    PWR_STATE_JITTER           = 4,    /* 电源抖动 */
    PWR_STATE_OVER_CURRENT     = 5,    /* 过流 */
} PowerState_t;

/**
 * @brief 电源极性状态
 */
typedef enum {
    POLARITY_POSITIVE          = 0,    /* 正接 */
    POLARITY_REVERSE           = 1,    /* 反接 */
    POLARITY_JITTER            = 2,    /* 抖动 */
    POLARITY_UNKNOWN           = 3,    /* 未识别 */
} PolarityState_t;

/**
 * @brief 电源配置参数（可存储 + 可默认）
 */
typedef struct {
    uint16_t voltage_rated;           /* 额定电压 mV */
    uint16_t voltage_min;             /* 欠压阈值 mV */
    uint16_t voltage_max;             /* 过压阈值 mV */
    uint16_t current_limit;           /* 过流阈值 mA */
    uint16_t voltage_debounce_ms;     /* 电压防抖窗口 */
    uint16_t polarity_debounce_ms;    /* 极性防抖窗口 */
} PowerConfig_t;

/**
 * @brief 电源实时信息
 */
typedef struct {
    PowerState_t state;              /* 电源状态 */
    PolarityState_t polarity;        /* 极性状态 */
    uint16_t voltage_now;            /* 当前电压 mV */
    uint16_t current_now;            /* 当前电流 mA */
} PowerInfo_t;

/* 对外接口 */
void Power_Init(void);
void Power_Task(void);
const PowerInfo_t* Power_GetInfo(void);

#endif /* __POWER_MANAGER_H */

