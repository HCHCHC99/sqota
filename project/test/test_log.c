#include "log_rtt.h"

void Test_Log(void)
{
    LOG_ERROR("日志功能测试：错误等级");
    LOG_WARN("日志功能测试：警告等级");
    LOG_INFO("日志功能测试：信息等级");
    LOG_DEBUG("日志功能测试：调试等级");
    LOG_INFO("日志颜色 & 分级 测试完成\n");
}

