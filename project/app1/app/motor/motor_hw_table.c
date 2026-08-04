
// /*===================== 硬件配置表：全局电机实例数组（核心查表） =====================*/
// /* 方法绑定快捷初始化：两种硬件绑定各自的函数指针 */
// #define MOTOR_OP_IO_UP_PWM_DOWN  {Motor_IoUpPwmDown_SetDir,Motor_IoUpPwmDown_SetDuty,Motor_Default_DeInit}
// #define MOTOR_OP_DOUBLE_PWM      {Motor_DoublePwm_SetDir,Motor_DoublePwm_SetDuty,Motor_Default_DeInit}

// /* 全局电机硬件表，所有电机硬件在此定义，新增电机仅扩展本表 */
// static MotorObj_t motor_hw_table[] = {
//     /* 电机0：上IO+下PWM 实例 */
//     {
//         .hw_type = MOTOR_HW_IO_UP_PWM_DOWN,
//         .op = MOTOR_OP_IO_UP_PWM_DOWN,
//         .hw_res.IoUpPwmDown = {
//             .up1 = {GPIOA,GPIO_PIN_0,GPIO_PIN_SET},
//             .up2 = {GPIOA,GPIO_PIN_1,GPIO_PIN_SET},
//             .down1 = {&htim1,TIM_CHANNEL_1,999},
//             .down2 = {&htim1,TIM_CHANNEL_2,999},
//         },
//         .cur_duty = 0,
//         .cur_dir = MOTOR_STOP
//     },
//     /* 电机1：全PWM上下桥实例 */
//     {
//         .hw_type = MOTOR_HW_DOUBLE_PWM,
//         .op = MOTOR_OP_DOUBLE_PWM,
//         .hw_res.DoublePwm = {
//             .up1 = {&htim2,TIM_CHANNEL_1,999},
//             .down1 = {&htim2,TIM_CHANNEL_2,999},
//             .up2 = {&htim2,TIM_CHANNEL_3,999},
//             .down2 = {&htim2,TIM_CHANNEL_4,999},
//         },
//         .cur_duty = 0,
//         .cur_dir = MOTOR_STOP
//     }
//     /* 后续新增电机直接在数组追加 */
// };
// #define MOTOR_COUNT (sizeof(motor_hw_table)/sizeof(MotorObj_t))
