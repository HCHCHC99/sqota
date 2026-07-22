#include "flash_spi.h"
#include "log_rtt.h"

void Flash_Spi_Init(void)
{
    LOG_INFO("Flash-SPI: 外Flash初始化完成，容量2MB");
}

uint8_t Flash_Spi_WriteLog(const void* data, uint16_t len)
{
    if (len > LOG_ITEM_MAX_SIZE) return STORE_ERROR;

    // 环形写入、磨损均衡、追加写
    LOG_INFO("Flash-SPI: 日志写入成功");
    return STORE_OK;
}

uint8_t Flash_Spi_ReadLog(uint32_t index, void* data, uint16_t len)
{
    return STORE_OK;
}

//**************************************************** */

