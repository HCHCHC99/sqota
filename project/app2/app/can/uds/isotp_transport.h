/*******************************************
* 文件名: isotp_transport.h
* 作者: AI Assistant
* 版本: V1.0.0
* 功能: ISO 15765-2 传输层协议实现
* 备注: 支持长报文的分包发送和接收
*******************************************/
#ifndef ISOTP_TRANSPORT_H_
#define ISOTP_TRANSPORT_H_

/***************************** 文件包含 *********************************/
#include "stdint.h"
#include "stdbool.h"

/***************************** 调试宏定义 *********************************/
// #define ISO_DEBUG
#ifdef ISO_DEBUG
    #define ISOTP_D(fmt, ...)  LOG_CH(LOG_CH_MAIN, LOG_LEVEL_DEBUG, COLOR_CYAN,   "ISOTP", fmt, ##__VA_ARGS__)
    #define ISOTP_I(fmt, ...)  LOG_CH(LOG_CH_MAIN, LOG_LEVEL_INFO,  COLOR_GREEN, "ISOTP", fmt, ##__VA_ARGS__)
    #define ISOTP_W(fmt, ...)  LOG_CH(LOG_CH_MAIN, LOG_LEVEL_WARN,  COLOR_YELLOW,"ISOTP", fmt, ##__VA_ARGS__)
    #define ISOTP_E(fmt, ...)  LOG_CH(LOG_CH_MAIN, LOG_LEVEL_ERROR, COLOR_RED,   "ISOTP", fmt, ##__VA_ARGS__)
#else
    #define ISOTP_D(fmt, ...)  (void)0
    #define ISOTP_I(fmt, ...)  (void)0
    #define ISOTP_W(fmt, ...)  (void)0
    #define ISOTP_E(fmt, ...)  (void)0
#endif

/* OTA 调试打印（打印关注的 CAN ID 帧，带序号） */
#define OTA_DEBUG
#ifdef OTA_DEBUG
    #define OTA_D(fmt, ...)  LOG_CH(LOG_CH_MAIN, LOG_LEVEL_DEBUG, COLOR_CYAN,   "OTA", fmt, ##__VA_ARGS__)
    #define OTA_I(fmt, ...)  LOG_CH(LOG_CH_MAIN, LOG_LEVEL_INFO,  COLOR_GREEN, "OTA", fmt, ##__VA_ARGS__)
    #define OTA_W(fmt, ...)  LOG_CH(LOG_CH_MAIN, LOG_LEVEL_WARN,  COLOR_YELLOW,"OTA", fmt, ##__VA_ARGS__)
    #define OTA_E(fmt, ...)  LOG_CH(LOG_CH_MAIN, LOG_LEVEL_ERROR, COLOR_RED,   "OTA", fmt, ##__VA_ARGS__)
#else
    #define OTA_D(fmt, ...)  (void)0
    #define OTA_I(fmt, ...)  (void)0
    #define OTA_W(fmt, ...)  (void)0
    #define OTA_E(fmt, ...)  (void)0
#endif

/***************************** CAN ID 过滤记录配置 ****************************/
/* 是否启用 CAN ID 过滤记录功能
 * 0: 禁用
 * 1: 启用
 */
#define ISOTP_ENABLE_FILTER_RECORD    1

/* 自动流控帧回复（已禁用，使用标准 ISO-TP 路径） */
// #define ISOTP_AUTO_FC

/* 关注的 CAN ID 列表（可任意增加） */
#define ISOTP_FILTER_CAN_ID_COUNT     4
#define ISOTP_FILTER_CAN_ID_LIST      {0x18DA03F1, 0x18DAF103, 0x18FF8118, 0x18DBFFF0}

/* 过滤记录缓冲区大小（环形缓冲区）*/
#define ISOTP_FILTER_BUFFER_SIZE      64

/****************************** 宏定义 ***********************************/

/* ISO-TP 帧类型标识 */
#define ISOTP_FRAME_SINGLE           0x00    /* 单帧 (SF) */
#define ISOTP_FRAME_FIRST            0x10    /* 首帧 (FF) */
#define ISOTP_FRAME_CONSECUTIVE      0x20    /* 连续帧 (CF) */
#define ISOTP_FRAME_FLOW_CONTROL     0x30    /* 流控帧 (FC) */

/* 流控状态 (Flow Status) */
#define ISOTP_FC_CTS                 0x00    /* Continue to Send, 继续发送 */
#define ISOTP_FC_WAIT                0x01    /* Wait, 等待 */
#define ISOTP_FC_OVERFLOW            0x02    /* Overflow, 溢出 */

/* 返回值 */
#define ISOTP_OK                     0       /* 成功 */
#define ISOTP_BUSY                   1       /* 忙（接收中） */
#define ISOTP_ERROR                 -1       /* 错误 */
#define ISOTP_TIMEOUT               -2       /* 超时 */
#define ISOTP_INCOMPLETE            -3       /* 不完整（等待更多数据） */

/* 配置参数 */
#define ISOTP_BUFFER_SIZE            8192    /* 接收缓冲区大小 (8KB) */
#define ISOTP_DEFAULT_RESPONSE_ID   0x18DAF103
#define ISOTP_RX_TIMEOUT_MS          65535   /* 接收超时时间 (ms) */

/* 流控参数 (保守模式) */
#define ISOTP_DEFAULT_BLOCK_SIZE     0       /* BS = 0 (一次性发送所有) */
#define ISOTP_DEFAULT_ST_MIN_MS      5       /* STmin = 5ms */

/* 最大支持的消息长度 */
#define ISOTP_MAX_MESSAGE_LEN        (4095)  /* ISO-TP 首帧最大支持 4095 字节 */

/************************** 数据类型及结构定义 ***************************/

/* ISO-TP 接收状态机 */
typedef enum
{
    ISOTP_RX_IDLE = 0,          /* 空闲，等待首帧 */
    ISOTP_RX_ACTIVE,            /* 接收中，正在接收连续帧 */
    ISOTP_RX_WAIT_FC,           /* 发送模式：等待流控帧 */
    ISOTP_RX_COMPLETE,          /* 接收完成 */
    ISOTP_RX_TIMEOUT            /* 接收超时 */
} isotp_rx_state_t;

/* ISO-TP 发送状态机 */
typedef enum
{
    ISOTP_TX_IDLE = 0,          /* 空闲 */
    ISOTP_TX_SENDING_FF,        /* 已发送首帧，等待流控或发送连续帧 */
    ISOTP_TX_SENDING_CF,        /* 发送连续帧中 */
    ISOTP_TX_COMPLETE,          /* 发送完成 */
    ISOTP_TX_TIMEOUT            /* 发送超时 */
} isotp_tx_state_t;

/* ISO-TP 连接结构体 (单连接，不支持并发) */
typedef struct
{
    /* 接收相关 */
    isotp_rx_state_t rx_state;          /* 接收状态 */
    uint32_t rx_src_id;                 /* 接收源 CAN ID (请求方) */
    uint32_t rx_dst_id;                 /* 接收目标 CAN ID (响应方) */
    uint8_t* rx_buffer;                 /* 接收缓冲区指针 */
    uint16_t rx_total_len;              /* 消息总长度 */
    uint16_t rx_received_len;           /* 已接收长度 */
    uint8_t rx_expected_seq;            /* 期望的下一个连续帧序号 (1-15) */
    uint8_t rx_cf_count_in_block;       /* 当前块内已接收的连续帧数 */
    uint16_t rx_timeout_counter;        /* 接收超时计数器 (ms) */
    
    /* 发送相关 */
    isotp_tx_state_t tx_state;          /* 发送状态 */
    uint32_t tx_dst_id;                 /* 发送目标 CAN ID */
    uint8_t* tx_buffer;                 /* 发送缓冲区指针 */
    uint16_t tx_total_len;              /* 发送消息总长度 */
    uint16_t tx_sent_len;               /* 已发送长度 */
    uint8_t tx_seq;                     /* 下一个连续帧序号 */
    uint8_t tx_cf_count_in_block;       /* 当前块内已发送的连续帧数 */
    uint16_t tx_timeout_counter;        /* 发送超时计数器 (ms) */
    uint8_t tx_bs;                      /* 对方要求的块大小 */
    uint8_t tx_st_min;                  /* 对方要求的最小间隔时间 */
    uint16_t tx_st_min_counter;         /* STmin 延迟计数器 */
    
    /* 配置参数 */
    uint8_t local_bs;                   /* 本端块大小 (发送流控时使用) */
    uint8_t local_st_min;               /* 本端最小间隔时间 (发送流控时使用) */
    uint16_t timeout_ms;                /* 超时时间 */
    
    /* 回调函数 */
    uint8_t channel;                    /* CAN 通道号 */
} isotp_connection_t;

/***************************** 函数声明 **********************************/

/* 初始化 ISO-TP 层 */
void isotp_init(uint8_t channel);

/* 1ms 定时器更新函数 (在外部 1ms 中断中调用) */
void isotp_ms_update(void);

/* 接收 CAN 帧 (在 CAN 接收回调中调用) */
int8_t isotp_receive_frame(uint8_t channel, uint32_t can_id, uint8_t* frame_data, 
                            uint8_t frame_len, uint8_t* out_data, uint16_t* out_len);

/* 发送完整消息 (自动拆分为 FF/CF) */
int8_t isotp_send_message(uint8_t channel, uint32_t dst_id, uint8_t* data, uint16_t len);

/* 发送处理函数 (需要在主循环中调用，处理发送状态机) */
void isotp_tx_process(void);

/* 重置接收状态 (用于错误恢复) */
void isotp_reset_rx(void);

/* 重置发送状态 */
void isotp_reset_tx(void);

/* 获取当前接收状态 */
isotp_rx_state_t isotp_get_rx_state(void);

/* 获取当前发送状态 */
isotp_tx_state_t isotp_get_tx_state(void);

/* 处理接收到的流控帧 (在 isotp_receive_frame 中调用) */
void isotp_handle_flow_control(uint8_t flow_status, uint8_t block_size, uint8_t st_min);

/* ==================== CAN ID 过滤记录调试接口 ==================== */
#if (ISOTP_ENABLE_FILTER_RECORD == 1)
/* 获取总记录次数 */
uint32_t isotp_get_filter_record_count(void);

/* 获取最后记录的 CAN ID */
uint32_t isotp_get_last_filtered_can_id(void);

/* 获取最后记录的数据 */
void isotp_get_last_filtered_data(uint8_t* out_data, uint8_t* out_len);

/* 获取指定索引的记录（0=最新，1=次新...）*/
bool isotp_get_filter_record(uint16_t index, uint32_t* can_id, uint8_t* data, uint8_t* len);
#endif

#endif /* ISOTP_TRANSPORT_H_ */
