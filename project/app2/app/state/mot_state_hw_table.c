#include "hc32f460.h"
#include "hc32_ll.h"
#include "hc32_ll_gpio.h"

#include "axis_typedef.h"

// M1_HU  PB7  电机1 上桥臂 IO控制  高电平 有效
// M1_HV  PA15 电机1 上桥臂 IO控制  高电平 有效

// M2_HU  PA6  电机2 上桥臂 IO控制  高电平 有效
// M2_HV  PA7  电机2 上桥臂 IO控制  高电平 有效
// ======================= 桥臂 硬件配置表 =======================
const Mot_HwConfig_t MOT_HW_TABLE[MOT_INDEX_MAX][ARM_INDEX_MAX] = {
    //  ARM_INDEX_HU----------------------------ARM_INDEX_HV
    {
        {GPIO_PORT_B,    GPIO_PIN_07,        1},{GPIO_PORT_A,    GPIO_PIN_15,        1}
    },    // 电机1
    {
        {GPIO_PORT_A,    GPIO_PIN_06,        1},{GPIO_PORT_A,    GPIO_PIN_07,        1}
    }     // 电机2
};

// ======================= 一个电机 硬件配置表 =======================
const Motor_Hw_t MOTOR_HW_LIST[MOT_INDEX_MAX] = {
    {
        .hu = &MOT_HW_TABLE[MOT_INDEX_0][ARM_INDEX_HU],
        .hv = &MOT_HW_TABLE[MOT_INDEX_0][ARM_INDEX_HV],
    },
    {
        .hu = &MOT_HW_TABLE[MOT_INDEX_1][ARM_INDEX_HU],
        .hv = &MOT_HW_TABLE[MOT_INDEX_1][ARM_INDEX_HV],
    },
};
