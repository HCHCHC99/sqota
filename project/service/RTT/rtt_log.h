#ifndef __RTT_LOG_CHANNEL_H
#define __RTT_LOG_CHANNEL_H

#include "SEGGER_RTT.h"

//====================================================================
// 1. 日志等级定义
//====================================================================
#define LOG_LEVEL_DEBUG    0
#define LOG_LEVEL_INFO     1
#define LOG_LEVEL_WARN     2
#define LOG_LEVEL_ERROR    3
#define LOG_LEVEL_FATAL    4

//====================================================================
// 2. 【多通道定义】给每个功能模块分配一个通道号
//====================================================================
typedef enum {
    LOG_CH_MAIN    = 0,    // 主程序
    LOG_CH_USB     = 0,    // USB 模块
    LOG_CH_SENSOR  = 0,    // 传感器
    LOG_CH_MOTOR   = 0,    // 电机
    LOG_CH_COMM    = 0,    // 通信
    LOG_CH_UI      = 0,    // 界面
    LOG_CH_MAX            // 通道总数
} LogChannel_t;


//====================================================================
// 3. 【全局配置】设置当前输出等级（低于此等级的不会打印）
//====================================================================
#define LOG_LEVEL_CONFIG   LOG_LEVEL_DEBUG   // 输出所有
// #define LOG_LEVEL_CONFIG   LOG_LEVEL_INFO    // 不输出DEBUG
// #define LOG_LEVEL_CONFIG   LOG_LEVEL_WARN    // 只输出警告/错误
// #define LOG_LEVEL_CONFIG   LOG_LEVEL_ERROR   // 只输出错误
// #define LOG_LEVEL_CONFIG   LOG_LEVEL_NONE    // 全部关闭


//====================================================================
// 4. 颜色定义（ANSI 转义码，RTT Viewer 完美支持）
//====================================================================
#define COLOR_CLEAR     "\033[0m"      // 清除颜色
#define COLOR_RED       "\033[31m"
#define COLOR_GREEN     "\033[32m"
#define COLOR_YELLOW    "\033[33m"
#define COLOR_BLUE      "\033[34m"
#define COLOR_PURPLE    "\033[35m"
#define COLOR_CYAN      "\033[36m"

//====================================================================
// 4. 【核心：带等级过滤的多通道打印宏】 目前 仅支持通道 0 
//====================================================================
#define LOG_CH(channel, level, color, tag, fmt, ...) \
    if (level >= LOG_LEVEL_CONFIG) { \
        SEGGER_RTT_printf(channel, color "[%s] " fmt COLOR_CLEAR "\r\n", tag, ##__VA_ARGS__); \
    }

//====================================================================
// 5. 给每个通道定义简化日志宏
//====================================================================

// ---------------------- 通道 0：主程序 MAIN ----------------------
#define MAIN_D(fmt, ...)  LOG_CH(LOG_CH_MAIN, LOG_LEVEL_DEBUG, COLOR_CYAN,   "MAIN", fmt, ##__VA_ARGS__)
#define MAIN_I(fmt, ...)  LOG_CH(LOG_CH_MAIN, LOG_LEVEL_INFO,  COLOR_GREEN, "MAIN", fmt, ##__VA_ARGS__)
#define MAIN_W(fmt, ...)  LOG_CH(LOG_CH_MAIN, LOG_LEVEL_WARN,  COLOR_YELLOW,"MAIN", fmt, ##__VA_ARGS__)
#define MAIN_E(fmt, ...)  LOG_CH(LOG_CH_MAIN, LOG_LEVEL_ERROR, COLOR_RED,   "MAIN", fmt, ##__VA_ARGS__)

// ---------------------- 通道 1：USB ----------------------
#define USB_D(fmt, ...)   LOG_CH(LOG_CH_USB, LOG_LEVEL_DEBUG, COLOR_CYAN,   "USB",  fmt, ##__VA_ARGS__)
#define USB_I(fmt, ...)   LOG_CH(LOG_CH_USB, LOG_LEVEL_INFO,  COLOR_GREEN, "USB",  fmt, ##__VA_ARGS__)
#define USB_W(fmt, ...)   LOG_CH(LOG_CH_USB, LOG_LEVEL_WARN,  COLOR_YELLOW,"USB",  fmt, ##__VA_ARGS__)
#define USB_E(fmt, ...)   LOG_CH(LOG_CH_USB, LOG_LEVEL_ERROR, COLOR_RED,   "USB",  fmt, ##__VA_ARGS__)

// ---------------------- 通道 2：传感器 SENSOR ----------------------
#define SENSOR_D(fmt, ...) LOG_CH(LOG_CH_SENSOR, LOG_LEVEL_DEBUG,COLOR_CYAN,  "SENSOR",fmt,##__VA_ARGS__)
#define SENSOR_I(fmt, ...) LOG_CH(LOG_CH_SENSOR, LOG_LEVEL_INFO, COLOR_GREEN,"SENSOR",fmt,##__VA_ARGS__)
#define SENSOR_W(fmt, ...) LOG_CH(LOG_CH_SENSOR, LOG_LEVEL_WARN, COLOR_YELLOW,"SENSOR",fmt,##__VA_ARGS__)
#define SENSOR_E(fmt, ...) LOG_CH(LOG_CH_SENSOR, LOG_LEVEL_ERROR,COLOR_RED,  "SENSOR",fmt,##__VA_ARGS__)

// ---------------------- 通道 3：电机 MOTOR ----------------------
#define MOTOR_D(fmt, ...) LOG_CH(LOG_CH_MOTOR, LOG_LEVEL_DEBUG,COLOR_CYAN,  "MOTOR",fmt,##__VA_ARGS__)
#define MOTOR_I(fmt, ...) LOG_CH(LOG_CH_MOTOR, LOG_LEVEL_INFO, COLOR_GREEN,"MOTOR",fmt,##__VA_ARGS__)
#define MOTOR_W(fmt, ...) LOG_CH(LOG_CH_MOTOR, LOG_LEVEL_WARN, COLOR_YELLOW,"MOTOR",fmt,##__VA_ARGS__)
#define MOTOR_E(fmt, ...) LOG_CH(LOG_CH_MOTOR, LOG_LEVEL_ERROR,COLOR_RED,  "MOTOR",fmt,##__VA_ARGS__)

// ---------------------- 通道 4：通信 COMM ----------------------
#define COMM_D(fmt, ...) LOG_CH(LOG_CH_COMM, LOG_LEVEL_DEBUG,COLOR_CYAN,  "COMM",fmt,##__VA_ARGS__)
#define COMM_I(fmt, ...) LOG_CH(LOG_CH_COMM, LOG_LEVEL_INFO, COLOR_GREEN,"COMM",fmt,##__VA_ARGS__)
#define COMM_W(fmt, ...) LOG_CH(LOG_CH_COMM, LOG_LEVEL_WARN, COLOR_YELLOW,"COMM",fmt,##__VA_ARGS__)
#define COMM_E(fmt, ...) LOG_CH(LOG_CH_COMM, LOG_LEVEL_ERROR,COLOR_RED,  "COMM",fmt,##__VA_ARGS__)

// ---------------------- 通道 5：UI ----------------------
#define UI_D(fmt, ...) LOG_CH(LOG_CH_UI, LOG_LEVEL_DEBUG,COLOR_CYAN,  "UI",fmt,##__VA_ARGS__)
#define UI_I(fmt, ...) LOG_CH(LOG_CH_UI, LOG_LEVEL_INFO, COLOR_GREEN,"UI",fmt,##__VA_ARGS__)
#define UI_W(fmt, ...) LOG_CH(LOG_CH_UI, LOG_LEVEL_WARN, COLOR_YELLOW,"UI",fmt,##__VA_ARGS__)
#define UI_E(fmt, ...) LOG_CH(LOG_CH_UI, LOG_LEVEL_ERROR,COLOR_RED,  "UI",fmt,##__VA_ARGS__)
//====================================================================
// 带等级 + 通道 + 文字颜色 + 背景色
//====================================================================


// // 文字颜色（前景色）
// #define FG_BLACK     "\033[30m"
// #define FG_RED       "\033[31m"
// #define FG_GREEN     "\033[32m"
// #define FG_YELLOW    "\033[33m"
// #define FG_BLUE      "\033[34m"
// #define FG_CYAN      "\033[36m"
// #define FG_WHITE     "\033[37m"

// // 背景色
// #define BG_BLACK     "\033[40m"
// #define BG_RED       "\033[41m"
// #define BG_GREEN     "\033[42m"
// #define BG_YELLOW    "\033[43m"
// #define BG_BLUE      "\033[44m"
// #define BG_CYAN      "\033[46m"
// #define BG_WHITE     "\033[47m"

// // 清除所有颜色（必须加在最后）
// #define ALL_CLEAR    "\033[0m"

// #define LOG_CH(channel, level, color, tag, fmt, ...) \
//     if (level >= LOG_LEVEL_CONFIG) { \
//         SEGGER_RTT_printf(channel, color "[%s] " fmt ALL_CLEAR "\r\n", tag, ##__VA_ARGS__); \
//     }

// //---------------------- 通道0：MAIN ----------------------
// #define MAIN_D(fmt, ...)  LOG_CH(0, LOG_LEVEL_DEBUG, FG_CYAN BG_BLACK,    "MAIN", fmt, ##__VA_ARGS__)
// #define MAIN_I(fmt, ...)  LOG_CH(0, LOG_LEVEL_INFO,  FG_BLACK BG_GREEN,   "MAIN", fmt, ##__VA_ARGS__)
// #define MAIN_W(fmt, ...)  LOG_CH(0, LOG_LEVEL_WARN,  FG_BLACK BG_YELLOW,  "MAIN", fmt, ##__VA_ARGS__)
// #define MAIN_E(fmt, ...)  LOG_CH(0, LOG_LEVEL_ERROR, FG_WHITE BG_RED,     "MAIN", fmt, ##__VA_ARGS__)

// //---------------------- 通道1：USB ----------------------
// #define USB_D(fmt, ...)   LOG_CH(1, LOG_LEVEL_DEBUG, FG_CYAN BG_BLACK,    "USB",  fmt, ##__VA_ARGS__)
// #define USB_I(fmt, ...)   LOG_CH(1, LOG_LEVEL_INFO,  FG_BLACK BG_GREEN,   "USB",  fmt, ##__VA_ARGS__)
// #define USB_W(fmt, ...)   LOG_CH(1, LOG_LEVEL_WARN,  FG_BLACK BG_YELLOW,  "USB",  fmt, ##__VA_ARGS__)
// #define USB_E(fmt, ...)   LOG_CH(1, LOG_LEVEL_ERROR, FG_WHITE BG_RED,     "USB",  fmt, ##__VA_ARGS__)

// // ---------------------- 通道 2：传感器 SENSOR ----------------------
// #define SENSOR_D(fmt, ...) LOG_CH(LOG_CH_SENSOR, LOG_LEVEL_DEBUG,COLOR_CYAN,  "SENSOR",fmt,##__VA_ARGS__)
// #define SENSOR_I(fmt, ...) LOG_CH(LOG_CH_SENSOR, LOG_LEVEL_INFO, COLOR_GREEN,"SENSOR",fmt,##__VA_ARGS__)
// #define SENSOR_W(fmt, ...) LOG_CH(LOG_CH_SENSOR, LOG_LEVEL_WARN, COLOR_YELLOW,"SENSOR",fmt,##__VA_ARGS__)
// #define SENSOR_E(fmt, ...) LOG_CH(LOG_CH_SENSOR, LOG_LEVEL_ERROR,COLOR_RED,  "SENSOR",fmt,##__VA_ARGS__)

// // ---------------------- 通道 3：电机 MOTOR ----------------------
// #define MOTOR_D(fmt, ...) LOG_CH(LOG_CH_MOTOR, LOG_LEVEL_DEBUG,COLOR_CYAN,  "MOTOR",fmt,##__VA_ARGS__)
// #define MOTOR_I(fmt, ...) LOG_CH(LOG_CH_MOTOR, LOG_LEVEL_INFO, COLOR_GREEN,"MOTOR",fmt,##__VA_ARGS__)
// #define MOTOR_W(fmt, ...) LOG_CH(LOG_CH_MOTOR, LOG_LEVEL_WARN, COLOR_YELLOW,"MOTOR",fmt,##__VA_ARGS__)
// #define MOTOR_E(fmt, ...) LOG_CH(LOG_CH_MOTOR, LOG_LEVEL_ERROR,COLOR_RED,  "MOTOR",fmt,##__VA_ARGS__)

// // ---------------------- 通道 4：通信 COMM ----------------------
// #define COMM_D(fmt, ...) LOG_CH(LOG_CH_COMM, LOG_LEVEL_DEBUG,COLOR_CYAN,  "COMM",fmt,##__VA_ARGS__)
// #define COMM_I(fmt, ...) LOG_CH(LOG_CH_COMM, LOG_LEVEL_INFO, COLOR_GREEN,"COMM",fmt,##__VA_ARGS__)
// #define COMM_W(fmt, ...) LOG_CH(LOG_CH_COMM, LOG_LEVEL_WARN, COLOR_YELLOW,"COMM",fmt,##__VA_ARGS__)
// #define COMM_E(fmt, ...) LOG_CH(LOG_CH_COMM, LOG_LEVEL_ERROR,COLOR_RED,  "COMM",fmt,##__VA_ARGS__)

// // ---------------------- 通道 5：UI ----------------------
// #define UI_D(fmt, ...) LOG_CH(LOG_CH_UI, LOG_LEVEL_DEBUG,COLOR_CYAN,  "UI",fmt,##__VA_ARGS__)
// #define UI_I(fmt, ...) LOG_CH(LOG_CH_UI, LOG_LEVEL_INFO, COLOR_GREEN,"UI",fmt,##__VA_ARGS__)
// #define UI_W(fmt, ...) LOG_CH(LOG_CH_UI, LOG_LEVEL_WARN, COLOR_YELLOW,"UI",fmt,##__VA_ARGS__)
// #define UI_E(fmt, ...) LOG_CH(LOG_CH_UI, LOG_LEVEL_ERROR,COLOR_RED,  "UI",fmt,##__VA_ARGS__)


#endif


