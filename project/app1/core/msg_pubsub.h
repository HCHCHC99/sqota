#ifndef __MSG_PUBSUB_H
#define __MSG_PUBSUB_H

#include <stdint.h>
#include "sys_config.h"

#if SYS_ENABLE_MSG_PUBSUB

// 消息优先级
#define MSG_PRIO_HIGH        0    // 故障、急停
#define MSG_PRIO_MID         1    // 控制指令、位置环
#define MSG_PRIO_LOW         2    // 状态、日志

// 消息回调函数类型
typedef void (*MsgCallback_t)(uint32_t topic, const void* data, uint16_t len, uint8_t prio);

// 初始化消息系统
void Msg_Init(void);

// 订阅消息（返回0成功，1失败）
uint8_t Msg_Subscribe(uint32_t topic, MsgCallback_t callback);

// 取消订阅
uint8_t Msg_Unsubscribe(uint32_t topic, MsgCallback_t callback);

// 发布消息（带可靠性检查）
void Msg_Publish(uint32_t topic, const void* data, uint16_t len, uint8_t prio);

// 获取当前订阅数
uint16_t Msg_GetSubscribeCount(void);

#endif

#endif


