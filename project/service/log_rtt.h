#ifndef __LOG_RTT_H
#define __LOG_RTT_H

#include <stdint.h>
#include "log_config.h"
#include "SEGGER_RTT.h"
#include "rtt_log.h"

////====================================================================
//// 1. 日志等级定义
////====================================================================
//#define LOG_LEVEL_DEBUG_    0
//#define LOG_LEVEL_INFO_     1
//#define LOG_LEVEL_WARN_     2
//#define LOG_LEVEL_ERROR_    3
//#define LOG_LEVEL_FATAL_    4

////====================================================================
//// 2. 【多通道定义】给每个功能模块分配一个通道号  目前只支持一个通道0
////====================================================================
//typedef enum {
//    LOG_CH_MAIN    = 0,    // 主程序
//    LOG_CH_USB     = 1,    // USB 模块
//    LOG_CH_SENSOR  = 2,    // 传感器
//    LOG_CH_MOTOR   = 3,    // 电机
//    LOG_CH_COMM    = 4,    // 通信
//    LOG_CH_UI      = 5,    // 界面
//    LOG_CH_MAX             // 通道总数
//} LogChannel_t;



////====================================================================
//// 3. RTT 颜色控制码（J-Link RTT Viewer 支持）
////====================================================================
//#define COLOR_RED       "\033[31m"
//#define COLOR_GREEN     "\033[32m"
//#define COLOR_YELLOW    "\033[33m"
//#define COLOR_BLUE      "\033[34m"
//#define COLOR_CLEAR     "\033[0m"  // 清除颜色

////====================================================================
//// 4. 核心打印宏（修复版）
////====================================================================
//#define LOG_CH(channel, level, color, tag, fmt, ...) \
//    SEGGER_RTT_printf(channel, color "%s " fmt COLOR_CLEAR "\r\n", tag, ##__VA_ARGS__)

// 初始化日志
void Log_Init(void);

//// 底层输出（带颜色）
//void Log_Print(uint8_t fg, uint8_t bg, const char *fmt, ...);

//====================================================================
// 4. 【核心：带等级过滤的多通道打印宏】 目前 仅支持通道 0 
//        SEGGER_RTT_printf(channel, color "[%s] " fmt COLOR_CLEAR "\r\n", tag, ##__VA_ARGS__); \
//====================================================================
#define LOG(channel, level, color, tag, fmt, ...) \
    if (level >= LOG_LEVEL_CONFIG) { \
			  SEGGER_RTT_printf(0, "%s " fmt "\r\n", tag, ##__VA_ARGS__);\
    }

// ====================== 分级日志接口 ======================
#if LOG_ENABLE

// ---------------------- 通道 0：主程序 MAIN ----------------------
#define LOG_ERROR(fmt, ...)  LOG(LOG_CH_MAIN, LOG_LEVEL_ERROR, COLOR_RED,      "[ERROR]", fmt, ##__VA_ARGS__)
#define LOG_WARN(fmt, ...)   LOG(LOG_CH_MAIN, LOG_LEVEL_WARN,  COLOR_YELLOW,   "[WARN]",  fmt, ##__VA_ARGS__)
#define LOG_INFO(fmt, ...)   LOG(LOG_CH_MAIN, LOG_LEVEL_INFO,  COLOR_GREEN,    "[INFO]",  fmt, ##__VA_ARGS__)
#define LOG_DEBUG(fmt, ...)  LOG(LOG_CH_MAIN, LOG_LEVEL_DEBUG, COLOR_CLEAR,    "[DEBUG]", fmt, ##__VA_ARGS__)


#else
#define LOG_ERROR(...)
#define LOG_WARN(...)
#define LOG_INFO(...)
#define LOG_DEBUG(...)
#endif

#endif

