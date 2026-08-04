#ifndef __HAL_BASE_H
#define __HAL_BASE_H

#include <stdint.h>
#include "sys_config.h"

// 状态返回
typedef enum {
    HAL_OK      = 0,
    HAL_ERROR   = 1,
} HalStatus_t;

// 高低电平
typedef enum {
    GPIO_PIN_RESET = 0,
    GPIO_PIN_SET   = 1,
} GPIO_PinState_t;

#endif

