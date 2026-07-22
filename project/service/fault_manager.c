#include "fault_manager.h"
#include "sys_config.h"
#include "log_rtt.h"
#include "msg_pubsub.h"
#include "msg_topics.h"
#include <string.h>

#if APP_ENABLE_FAULT_MANAGE

static uint8_t s_current_fault = FAULT_NONE;

// ------------------------------
// 初始化故障管理
// ------------------------------
void Fault_Init(void)
{
    s_current_fault = FAULT_NONE;
    LOG_INFO("Fault Manager init OK");
}

// ------------------------------
// 故障管理任务（1ms周期）
// ------------------------------
void Fault_Task(void)
{
    if (s_current_fault != FAULT_NONE) {
        // 发布故障消息
        Msg_Publish(TOPIC_ACTUATOR_FAULT, &s_current_fault, sizeof(uint8_t), MSG_PRIO_HIGH);
        
        // 故障只上报一次，直到被清除
        LOG_ERROR("Fault active: 0x%02X", s_current_fault);
    }
}

// ------------------------------
// 上报故障
// ------------------------------
void Fault_Report(uint8_t fault_code)
{
    if (fault_code == FAULT_NONE || fault_code == s_current_fault) {
        return;
    }
    
    s_current_fault = fault_code;
    LOG_ERROR("Fault reported: 0x%02X", fault_code);
}

// ------------------------------
// 清除故障
// ------------------------------
void Fault_Clear(uint8_t fault_code)
{
    if (fault_code == FAULT_NONE) {
        s_current_fault = FAULT_NONE;
    } else if (s_current_fault == fault_code) {
        s_current_fault = FAULT_NONE;
    }
    LOG_INFO("Fault cleared: 0x%02X", fault_code);
}

// ------------------------------
// 获取当前故障码
// ------------------------------
uint8_t Fault_GetCurrent(void)
{
    return s_current_fault;
}

#endif

