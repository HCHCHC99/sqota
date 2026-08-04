#ifndef __FLASH_SPI_H
#define __FLASH_SPI_H

#include <stdint.h>
#include "sys_config.h"
#include "storage.h"

#define LOG_ITEM_MAX_SIZE      64

void Flash_Spi_Init(void);
uint8_t Flash_Spi_WriteLog(const void* data, uint16_t len);
uint8_t Flash_Spi_ReadLog(uint32_t index, void* data, uint16_t len);



//******************************************8888*/
#endif

