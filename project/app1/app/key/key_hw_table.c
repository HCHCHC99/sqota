#include "key.h"

//M1_KEY  PB9  按键1             低电平 有效
//M2_KEY  PB8  按键2             低电平 有效

//ACC_KEY PB10 外部 ACC信号      低电平 有效
//JH_KEY  PB2  外部 结合信号      低电平 有效
//FL_KEY  PB1  外部 分离信号      低电平 有效
//GDW_KEY PB0  外部 高档位信号    低电平 有效


const Key_HwConfig_t KEY_HW_TABLE[] = {
    // M1_KEY
    {
        .gpio = GPIO_PORT_B,
        .pin = GPIO_PIN_09,
        .active_level = 0,
        .long_press_ms = 200,
    },
    // M2_KEY
    {
        .gpio = GPIO_PORT_B,
        .pin = GPIO_PIN_08,
        .active_level = 0,
        .long_press_ms = 200,
    },
    // ACC_KEY
    {
        .gpio = GPIO_PORT_B,
        .pin = GPIO_PIN_10,
        .active_level = 0,
        .long_press_ms = 200,
    },
    // JH_KEY
    {
        .gpio = GPIO_PORT_B,
        .pin = GPIO_PIN_02,
        .active_level = 0,
        .long_press_ms = 200,
    },
    // FL_KEY
    {
        .gpio = GPIO_PORT_B,
        .pin = GPIO_PIN_01,
        .active_level = 0,
        .long_press_ms = 200,
    },
    // GDW_KEY
    {
        .gpio = GPIO_PORT_B,
        .pin = GPIO_PIN_00,
        .active_level = 0,
        .long_press_ms = 200,
    },

};

// const uint8_t KEY_COUNT = sizeof(KEY_HW_TABLE) / sizeof(Key_HwConfig_t);


