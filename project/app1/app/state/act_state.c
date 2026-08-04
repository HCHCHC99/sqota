/**
 * @file    act_state.c
 * @brief   推杆状态机实现
 * @note    负责推杆运动逻辑，下发指令至电机
 */
#include "act_state.h"
#include "event_def.h"
#include "msg_pubsub.h"
#include "axis_typedef.h"
#include "hc32_ll_utility.h" //260609_RL_add:

/* 全局系统对象 */
extern System_t mySystem;

///**
// * @brief 推杆状态枚举（完全私有，对外不可见）
// */
//typedef enum {
//    ACT_STATE_INIT = 0,                   // 初始化状态
//    ACT_STATE_IDLE ,                      // 空闲状态
//   
//    ACT_STATE_COMBINE = 2,
//    ACT_STATE_COMBINE_TRY_1,
//    ACT_STATE_COMBINE_TRY_2,
//    ACT_STATE_COMBINE_TRY_3,
//    ACT_STATE_COMBINE_BACK = 6,
//    ACT_STATE_COMBINE_BACK_1,
//    ACT_STATE_COMBINE_BACK_2,
//    ACT_STATE_COMBINE_SUCCESS,
//    ACT_STATE_COMBINE_FAIL = 10,

//    
//    ACT_STATE_SEPARATE =11,
//    ACT_STATE_SEPARATE_TRY_1,
//    ACT_STATE_SEPARATE_TRY_2,
//    ACT_STATE_SEPARATE_TRY_3,
//    ACT_STATE_SEPARATE_BACK = 15,
//    ACT_STATE_SEPARATE_BACK_1,
//    ACT_STATE_SEPARATE_BACK_2,
//    ACT_STATE_SEPARATE_SUCCESS,
//    ACT_STATE_SEPARATE_FAIL = 19,


//    ACT_STATE_DISABLE = 20,
//    ACT_STATE_HOLD,                     // 保持/制动
//    ACT_STATE_ERROR = 21,               // 错误
//    
//    ACT_STATE_CALIBRATION = 22,             //260616_RL_add：标定  
//    ACT_STATE_CALIBRATION_OUT,              //260616_RL_add：标定伸出
//    ACT_STATE_CALIBRATION_IN,               //260616_RL_add：标定伸出
//} ActuatorState_t;

/* 状态入口函数声明 */
static void act_enter_idle(void);
static void act_enter_combine(void);
static void act_enter_moving(void);
static void act_enter_separate(void);
static void act_enter_hold(void);
// static void act_enter_stop(void);
static void act_enter_calibration(void);   //260606_RL_add: 标定执行函数

/**
 * @brief 推杆状态跳转表（全枚举定义）
 */
static const StateJumpTable_t act_jump[] = {

    {ACT_STATE_INIT,             EVT_ACT_WORK_ENABLE,      ACT_STATE_IDLE},

    {ACT_STATE_IDLE,             EVT_ACT_WORK_DISABLE,     ACT_STATE_DISABLE},
    {ACT_STATE_IDLE,             EVT_ACT_COMBINE,          ACT_STATE_COMBINE},
    {ACT_STATE_IDLE,             EVT_ACT_SEPARATE,         ACT_STATE_SEPARATE},    
    {ACT_STATE_IDLE,             EVT_ACT_HOLD,             ACT_STATE_HOLD},
    {ACT_STATE_IDLE,             EVT_ACT_ERROR,            ACT_STATE_ERROR},

   //  {ACT_STATE_HOLD,             EVT_ACT_COMBINE,          ACT_STATE_COMBINE},
   //  {ACT_STATE_HOLD,             EVT_ACT_COMBINE_BACK,         ACT_STATE_COMBINE_BACK},
   //  {ACT_STATE_HOLD,             EVT_ACT_SEPARATE,         ACT_STATE_SEPARATE},
   //  {ACT_STATE_HOLD,             EVT_ACT_SEPARATE,         ACT_STATE_SEPARATE},
   //  {ACT_STATE_HOLD,             EVT_ACT_COMBINE_SUCCESS,  ACT_STATE_COMBINE_SUCCESS},
   //  {ACT_STATE_HOLD,             EVT_ACT_COMBINE_BACK,     ACT_STATE_COMBINE_BACK},
   //  {ACT_STATE_HOLD,             EVT_ACT_HOLD,             ACT_STATE_HOLD},
   //  {ACT_STATE_HOLD,             EVT_ACT_ERROR,            ACT_STATE_ERROR},
   //  {ACT_STATE_HOLD,             EVT_ACT_WORK_DISABLE,     ACT_STATE_DISABLE},

    {ACT_STATE_COMBINE,          EVT_ACT_SEPARATE,         ACT_STATE_SEPARATE},
    {ACT_STATE_COMBINE,          EVT_ACT_COMBINE_SUCCESS,  ACT_STATE_COMBINE_SUCCESS},
    {ACT_STATE_COMBINE,          EVT_ACT_COMBINE_BACK,     ACT_STATE_COMBINE_BACK},
    {ACT_STATE_COMBINE,          EVT_ACT_HOLD,             ACT_STATE_HOLD},
    {ACT_STATE_COMBINE,          EVT_ACT_ERROR,            ACT_STATE_ERROR},
    {ACT_STATE_COMBINE,          EVT_ACT_WORK_DISABLE,     ACT_STATE_DISABLE},

    {ACT_STATE_COMBINE_BACK,     EVT_ACT_SEPARATE,         ACT_STATE_SEPARATE},
    {ACT_STATE_COMBINE_BACK,     EVT_ACT_COMBINE,          ACT_STATE_COMBINE_TRY_1},
    {ACT_STATE_COMBINE_BACK,     EVT_ACT_HOLD,             ACT_STATE_HOLD},
    {ACT_STATE_COMBINE_BACK,     EVT_ACT_ERROR,            ACT_STATE_ERROR},
    {ACT_STATE_COMBINE_BACK,     EVT_ACT_WORK_DISABLE,     ACT_STATE_DISABLE},

    {ACT_STATE_COMBINE_TRY_1,    EVT_ACT_SEPARATE,         ACT_STATE_SEPARATE},
    {ACT_STATE_COMBINE_TRY_1,    EVT_ACT_COMBINE_BACK,     ACT_STATE_COMBINE_BACK_1},
    {ACT_STATE_COMBINE_TRY_1,    EVT_ACT_HOLD,             ACT_STATE_HOLD},
    {ACT_STATE_COMBINE_TRY_1,    EVT_ACT_ERROR,            ACT_STATE_ERROR},
    {ACT_STATE_COMBINE_TRY_1,    EVT_ACT_WORK_DISABLE,     ACT_STATE_DISABLE},
    {ACT_STATE_COMBINE_TRY_1,    EVT_ACT_COMBINE_SUCCESS,  ACT_STATE_COMBINE_SUCCESS},      //260611_RL_add:尝试时可以成功

    {ACT_STATE_COMBINE_BACK_1,   EVT_ACT_SEPARATE,         ACT_STATE_SEPARATE},
    {ACT_STATE_COMBINE_BACK_1,   EVT_ACT_COMBINE,          ACT_STATE_COMBINE_TRY_2},
    {ACT_STATE_COMBINE_BACK_1,   EVT_ACT_HOLD,             ACT_STATE_HOLD},
    {ACT_STATE_COMBINE_BACK_1,   EVT_ACT_ERROR,            ACT_STATE_ERROR},
    {ACT_STATE_COMBINE_BACK_1,   EVT_ACT_WORK_DISABLE,     ACT_STATE_DISABLE},

    {ACT_STATE_COMBINE_TRY_2,    EVT_ACT_SEPARATE,         ACT_STATE_SEPARATE},
    {ACT_STATE_COMBINE_TRY_2,    EVT_ACT_COMBINE_BACK,     ACT_STATE_COMBINE_BACK_2},
    {ACT_STATE_COMBINE_TRY_2,    EVT_ACT_HOLD,             ACT_STATE_HOLD},
    {ACT_STATE_COMBINE_TRY_2,    EVT_ACT_ERROR,            ACT_STATE_ERROR},
    {ACT_STATE_COMBINE_TRY_2,    EVT_ACT_WORK_DISABLE,     ACT_STATE_DISABLE},
    {ACT_STATE_COMBINE_TRY_2,    EVT_ACT_COMBINE_SUCCESS,  ACT_STATE_COMBINE_SUCCESS},      //260611_RL_add:尝试时可以成功

    {ACT_STATE_COMBINE_BACK_2,   EVT_ACT_SEPARATE,         ACT_STATE_SEPARATE},
    {ACT_STATE_COMBINE_BACK_2,   EVT_ACT_COMBINE,          ACT_STATE_COMBINE_TRY_3},
    {ACT_STATE_COMBINE_BACK_2,   EVT_ACT_HOLD,             ACT_STATE_HOLD},
    {ACT_STATE_COMBINE_BACK_2,   EVT_ACT_ERROR,            ACT_STATE_ERROR},
    {ACT_STATE_COMBINE_BACK_2,   EVT_ACT_WORK_DISABLE,     ACT_STATE_DISABLE},

    {ACT_STATE_COMBINE_TRY_3,    EVT_ACT_COMBINE_BACK,     ACT_STATE_COMBINE_FAIL},
    {ACT_STATE_COMBINE_TRY_3,    EVT_ACT_HOLD,             ACT_STATE_HOLD},
    {ACT_STATE_COMBINE_TRY_3,    EVT_ACT_ERROR,            ACT_STATE_ERROR},
    {ACT_STATE_COMBINE_TRY_3,    EVT_ACT_WORK_DISABLE,     ACT_STATE_DISABLE},
    {ACT_STATE_COMBINE_TRY_3,    EVT_ACT_COMBINE_SUCCESS,  ACT_STATE_COMBINE_SUCCESS},      //260611_RL_add:尝试时可以成功

    {ACT_STATE_SEPARATE,         EVT_ACT_COMBINE,          ACT_STATE_COMBINE},
    {ACT_STATE_SEPARATE,         EVT_ACT_SEPARATE_SUCCESS, ACT_STATE_SEPARATE_SUCCESS},
    {ACT_STATE_SEPARATE,         EVT_ACT_SEPARATE_BACK,    ACT_STATE_SEPARATE_BACK},
    {ACT_STATE_SEPARATE,         EVT_ACT_HOLD,             ACT_STATE_HOLD},
    {ACT_STATE_SEPARATE,         EVT_ACT_ERROR,            ACT_STATE_ERROR},
    {ACT_STATE_SEPARATE,         EVT_ACT_WORK_DISABLE,     ACT_STATE_DISABLE},

    {ACT_STATE_SEPARATE_BACK,    EVT_ACT_COMBINE,          ACT_STATE_COMBINE},
    {ACT_STATE_SEPARATE_BACK,    EVT_ACT_SEPARATE,         ACT_STATE_SEPARATE_TRY_1},
    {ACT_STATE_SEPARATE_BACK,    EVT_ACT_HOLD,             ACT_STATE_HOLD},
    {ACT_STATE_SEPARATE_BACK,    EVT_ACT_ERROR,            ACT_STATE_ERROR},
    {ACT_STATE_SEPARATE_BACK,    EVT_ACT_WORK_DISABLE,     ACT_STATE_DISABLE},

    {ACT_STATE_SEPARATE_TRY_1,   EVT_ACT_COMBINE,          ACT_STATE_COMBINE},
    {ACT_STATE_SEPARATE_TRY_1,   EVT_ACT_SEPARATE_BACK,    ACT_STATE_SEPARATE_BACK_1},
    {ACT_STATE_SEPARATE_TRY_1,   EVT_ACT_HOLD,             ACT_STATE_HOLD},
    {ACT_STATE_SEPARATE_TRY_1,   EVT_ACT_ERROR,            ACT_STATE_ERROR},
    {ACT_STATE_SEPARATE_TRY_1,   EVT_ACT_WORK_DISABLE,     ACT_STATE_DISABLE},
    {ACT_STATE_SEPARATE_TRY_1,   EVT_ACT_SEPARATE_SUCCESS, ACT_STATE_SEPARATE_SUCCESS},      //260611_RL_add:尝试时可以成功

    {ACT_STATE_SEPARATE_BACK_1,  EVT_ACT_COMBINE,          ACT_STATE_COMBINE},
    {ACT_STATE_SEPARATE_BACK_1,  EVT_ACT_SEPARATE,         ACT_STATE_SEPARATE_TRY_2},
    {ACT_STATE_SEPARATE_BACK_1,  EVT_ACT_HOLD,             ACT_STATE_HOLD},
    {ACT_STATE_SEPARATE_BACK_1,  EVT_ACT_ERROR,            ACT_STATE_ERROR},
    {ACT_STATE_SEPARATE_BACK_1,  EVT_ACT_WORK_DISABLE,     ACT_STATE_DISABLE},

    {ACT_STATE_SEPARATE_TRY_2,   EVT_ACT_COMBINE,          ACT_STATE_COMBINE},
    {ACT_STATE_SEPARATE_TRY_2,   EVT_ACT_SEPARATE_BACK,    ACT_STATE_SEPARATE_BACK_2},
    {ACT_STATE_SEPARATE_TRY_2,   EVT_ACT_HOLD,             ACT_STATE_HOLD},
    {ACT_STATE_SEPARATE_TRY_2,   EVT_ACT_ERROR,            ACT_STATE_ERROR},
    {ACT_STATE_SEPARATE_TRY_2,   EVT_ACT_WORK_DISABLE,     ACT_STATE_DISABLE},
    {ACT_STATE_SEPARATE_TRY_2,   EVT_ACT_SEPARATE_SUCCESS, ACT_STATE_SEPARATE_SUCCESS},      //260611_RL_add:尝试时可以成功

    {ACT_STATE_SEPARATE_BACK_2,  EVT_ACT_COMBINE,          ACT_STATE_COMBINE},
    {ACT_STATE_SEPARATE_BACK_2,  EVT_ACT_SEPARATE,         ACT_STATE_SEPARATE_TRY_3},
    {ACT_STATE_SEPARATE_BACK_2,  EVT_ACT_HOLD,             ACT_STATE_HOLD},
    {ACT_STATE_SEPARATE_BACK_2,  EVT_ACT_ERROR,            ACT_STATE_ERROR},
    {ACT_STATE_SEPARATE_BACK_2,  EVT_ACT_WORK_DISABLE,     ACT_STATE_DISABLE},

    {ACT_STATE_SEPARATE_TRY_3,   EVT_ACT_COMBINE,          ACT_STATE_COMBINE},
    {ACT_STATE_SEPARATE_TRY_3,   EVT_ACT_SEPARATE_BACK,    ACT_STATE_SEPARATE_FAIL},
    {ACT_STATE_SEPARATE_TRY_3,   EVT_ACT_HOLD,             ACT_STATE_HOLD},
    {ACT_STATE_SEPARATE_TRY_3,   EVT_ACT_ERROR,            ACT_STATE_ERROR},
    {ACT_STATE_SEPARATE_TRY_3,   EVT_ACT_WORK_DISABLE,     ACT_STATE_DISABLE},
    {ACT_STATE_SEPARATE_TRY_3,   EVT_ACT_SEPARATE_SUCCESS, ACT_STATE_SEPARATE_SUCCESS},      //260611_RL_add:尝试时可以成功

    {ACT_STATE_HOLD,             EVT_ACT_WORK_ENABLE,      ACT_STATE_IDLE},
    {ACT_STATE_ERROR,            EVT_ACT_RESET,            ACT_STATE_INIT},
    {ACT_STATE_DISABLE,          EVT_ACT_WORK_ENABLE,      ACT_STATE_IDLE},

    //260611_RL_add: 测试方便，增加结合成功/失败,发送复位事件，跳到空闲状态。 用完根据实际情况修改、删除！
    {ACT_STATE_SEPARATE_SUCCESS, EVT_ACT_RESET,            ACT_STATE_IDLE},
    {ACT_STATE_COMBINE_SUCCESS,  EVT_ACT_RESET,            ACT_STATE_IDLE},
    {ACT_STATE_COMBINE_FAIL,     EVT_ACT_RESET,            ACT_STATE_IDLE},
    {ACT_STATE_SEPARATE_FAIL,    EVT_ACT_RESET,            ACT_STATE_IDLE},
    
    
    //260616_RL_add: 增加标定装态下的状态机跳转
    {ACT_STATE_IDLE,             EVT_ACT_CALIBRATION_ENTER,     ACT_STATE_CALIBRATION},
    
    {ACT_STATE_CALIBRATION,      EVT_ACT_CALIBRATION_OUT,       ACT_STATE_CALIBRATION_OUT},
    {ACT_STATE_CALIBRATION,      EVT_ACT_CALIBRATION_IN,        ACT_STATE_CALIBRATION_IN},
    {ACT_STATE_CALIBRATION,      EVT_ACT_CALIBRATION_SUCCESS,   ACT_STATE_SEPARATE_SUCCESS},        //标定成功，跳转到已分离    
    {ACT_STATE_CALIBRATION,      EVT_ACT_CALIBRATION_FALL,      ACT_STATE_IDLE},                    //标定失败，跳转标定到前状态
    
    {ACT_STATE_CALIBRATION_OUT,  EVT_ACT_CALIBRATION_STOP,      ACT_STATE_CALIBRATION},
    
    {ACT_STATE_CALIBRATION_IN,   EVT_ACT_CALIBRATION_STOP,      ACT_STATE_CALIBRATION},

};

/*
测试方便查看事件，测完删除
--------------------- 推杆事件组 事件定义---------------------
#define EVT_ACT_WORK_ENABLE            (1U << 0)    // 推杆 工作使能 1
#define EVT_ACT_WORK_DISABLE           (1U << 1)    // 推杆 工作禁止 2

#define EVT_ACT_COMBINE                (1U << 2)    // 推杆 结合    4
#define EVT_ACT_COMBINE_TRY_1          (1U << 3)    // 推杆 结合 尝试1 8
#define EVT_ACT_COMBINE_TRY_2          (1U << 4)    // 推杆 结合 尝试2 16
#define EVT_ACT_COMBINE_TRY_3          (1U << 5)    // 推杆 结合 尝试3 32
#define EVT_ACT_COMBINE_BACK           (1U << 6)    // 推杆 结合 回退  64
#define EVT_ACT_COMBINE_SUCCESS        (1U << 7)    // 推杆 结合 成功  128
#define EVT_ACT_COMBINE_FAIL           (1U << 8)    // 推杆 结合 失败  256

#define EVT_ACT_SEPARATE               (1U << 9)    // 推杆 分离    512
#define EVT_ACT_SEPARATE_TRY_1         (1U << 10)    // 推杆 分离 尝试1 1024
#define EVT_ACT_SEPARATE_TRY_2         (1U << 11)    // 推杆 分离 尝试2 2048
#define EVT_ACT_SEPARATE_TRY_3         (1U << 12)    // 推杆 分离 尝试3 4096
#define EVT_ACT_SEPARATE_BACK          (1U << 13)    // 推杆 分离 回退  8192
#define EVT_ACT_SEPARATE_SUCCESS       (1U << 14)    // 推杆 分离 成功  16384
#define EVT_ACT_SEPARATE_FAIL          (1U << 15)    // 推杆 分离 失败 32768

#define EVT_ACT_HOLD                   (1U << 16)     // 推杆 保持 65536
#define EVT_ACT_ERROR                  (1U << 17)     // 推杆 错误 131072
#define EVT_ACT_RESET                  (1U << 18)     // 推杆 复位 262144

#define EVT_ACT_HOLD                   (1U << 16)     // 推杆 保持 65536
#define EVT_ACT_ERROR                  (1U << 17)     // 推杆 错误 131072
#define EVT_ACT_RESET                  (1U << 18)     // 推杆 复位 262144

#define EVT_ACT_CALIBRATION_START      (1U << 19)     // 推杆 开始标定 524288
#define EVT_ACT_CALIBRATION_OUT        (1U << 20)     // 推杆 标定伸出 1048576
#define EVT_ACT_CALIBRATION_IN         (1U << 21)     // 推杆 停止缩回 2097152
#define EVT_ACT_CALIBRATION_STOP       (1U << 22)     // 推杆 停止标定 4194304


**/

/**
 * @brief 推杆状态入口函数表
 */
static const StateFuncTable_t act_func[] = {
   {ACT_STATE_INIT,                act_enter_idle,        0},
   {ACT_STATE_IDLE,                act_enter_idle,        0},

   {ACT_STATE_COMBINE,             act_enter_moving,      0},
   {ACT_STATE_COMBINE_TRY_1,       act_enter_moving,      0},
   {ACT_STATE_COMBINE_TRY_2,       act_enter_moving,      0},
   {ACT_STATE_COMBINE_TRY_3,       act_enter_moving,      0},
   {ACT_STATE_COMBINE_BACK,        act_enter_moving,      0},
   {ACT_STATE_COMBINE_BACK_1,      act_enter_moving,      0},
   {ACT_STATE_COMBINE_BACK_2,      act_enter_moving,      0},
   {ACT_STATE_COMBINE_SUCCESS,     act_enter_idle,        0},
   {ACT_STATE_COMBINE_FAIL,        act_enter_idle,        0},

   {ACT_STATE_SEPARATE,            act_enter_moving,      0},
   {ACT_STATE_SEPARATE_TRY_1,      act_enter_moving,      0},
   {ACT_STATE_SEPARATE_TRY_2,      act_enter_moving,      0},
   {ACT_STATE_SEPARATE_TRY_3,      act_enter_moving,      0},
   {ACT_STATE_SEPARATE_BACK,       act_enter_moving,      0},
   {ACT_STATE_SEPARATE_BACK_1,     act_enter_moving,      0},
   {ACT_STATE_SEPARATE_BACK_2,     act_enter_moving,      0},
   {ACT_STATE_SEPARATE_SUCCESS,    act_enter_idle,        0},
   {ACT_STATE_SEPARATE_FAIL,       act_enter_idle,        0},

   {ACT_STATE_DISABLE,             act_enter_hold,        0},
   {ACT_STATE_HOLD,                act_enter_hold,        0},
   {ACT_STATE_ERROR,               act_enter_hold,        0},

   //260616_RL_add: 标定过程。单独函数实现
   {ACT_STATE_CALIBRATION,         act_enter_calibration,        0},
   {ACT_STATE_CALIBRATION_OUT,     act_enter_calibration,        0},    
   {ACT_STATE_CALIBRATION_IN,      act_enter_calibration,        0},  
};

/**
 * @brief 推杆状态机 初始化
 * @note  
 */
void Act_State_Init(StateMachine_t *sm)
{
    StateMachine_t *sm_temp = sm;
    sm_temp->jump_table = act_jump;
    sm_temp->jump_table_size = sizeof(act_jump)/sizeof(StateJumpTable_t);
    sm_temp->func_table = act_func;
    sm_temp->func_table_size = sizeof(act_func)/sizeof(StateFuncTable_t);
    sm_temp->init_state = ACT_STATE_IDLE;

    StateMachine_Init(sm_temp);
}
/**
 * @brief 推杆状态机运行
 * @param  axis: 轴对象
 * @note   处理事件，下发电动机指令
 */
void Act_State_Task(Axis_t *axis)
{
   EventBits_t bits = EventGroup_Get(axis->evt_act);
   
   switch (bits)
   {
   case EVT_ACT_WORK_ENABLE: //  推杆 工作使能
       StateMachine_SendEvent(&axis->sm_act, EVT_ACT_WORK_ENABLE);
       EventGroup_Clear(axis->evt_act, EVT_ACT_WORK_ENABLE);
       axis->enable = 1;

    break;
   case EVT_ACT_WORK_DISABLE://  推杆 工作禁止
       StateMachine_SendEvent(&axis->sm_act, EVT_ACT_WORK_DISABLE);
       EventGroup_Clear(axis->evt_act, EVT_ACT_WORK_DISABLE);
       axis->enable = 0;
    break;
   case EVT_ACT_COMBINE://  推杆 结合
       StateMachine_SendEvent(&axis->sm_act, EVT_ACT_COMBINE);
       EventGroup_Clear(axis->evt_act, EVT_ACT_COMBINE);
    break;
   case EVT_ACT_COMBINE_TRY_1:
       StateMachine_SendEvent(&axis->sm_act, EVT_ACT_COMBINE_TRY_1);
       EventGroup_Clear(axis->evt_act, EVT_ACT_COMBINE_TRY_1);
    break;
   case EVT_ACT_COMBINE_TRY_2:
       StateMachine_SendEvent(&axis->sm_act, EVT_ACT_COMBINE_TRY_2);
       EventGroup_Clear(axis->evt_act, EVT_ACT_COMBINE_TRY_2);
    break;
   case EVT_ACT_COMBINE_TRY_3:
       StateMachine_SendEvent(&axis->sm_act, EVT_ACT_COMBINE_TRY_3);
       EventGroup_Clear(axis->evt_act, EVT_ACT_COMBINE_TRY_3);
    break;
   case EVT_ACT_COMBINE_BACK:
       StateMachine_SendEvent(&axis->sm_act, EVT_ACT_COMBINE_BACK);
       EventGroup_Clear(axis->evt_act, EVT_ACT_COMBINE_BACK);
    break;
   case EVT_ACT_COMBINE_SUCCESS:
       StateMachine_SendEvent(&axis->sm_act, EVT_ACT_COMBINE_SUCCESS);
       EventGroup_Clear(axis->evt_act, EVT_ACT_COMBINE_SUCCESS);
    break;
   case EVT_ACT_COMBINE_FAIL:
       StateMachine_SendEvent(&axis->sm_act, EVT_ACT_COMBINE_FAIL);
       EventGroup_Clear(axis->evt_act, EVT_ACT_COMBINE_FAIL);
    break;
   case EVT_ACT_SEPARATE:
       StateMachine_SendEvent(&axis->sm_act, EVT_ACT_SEPARATE);
       EventGroup_Clear(axis->evt_act, EVT_ACT_SEPARATE);
    break;
   case EVT_ACT_SEPARATE_TRY_1:
       StateMachine_SendEvent(&axis->sm_act, EVT_ACT_SEPARATE_TRY_1);
       EventGroup_Clear(axis->evt_act, EVT_ACT_SEPARATE_TRY_1);
    break;
   case EVT_ACT_SEPARATE_TRY_2:
       StateMachine_SendEvent(&axis->sm_act, EVT_ACT_SEPARATE_TRY_2);
       EventGroup_Clear(axis->evt_act, EVT_ACT_SEPARATE_TRY_2);
    break;
   case EVT_ACT_SEPARATE_TRY_3:
       StateMachine_SendEvent(&axis->sm_act, EVT_ACT_SEPARATE_TRY_3);
       EventGroup_Clear(axis->evt_act, EVT_ACT_SEPARATE_TRY_3);
    break;
   case EVT_ACT_SEPARATE_BACK:
       StateMachine_SendEvent(&axis->sm_act, EVT_ACT_SEPARATE_BACK);
       EventGroup_Clear(axis->evt_act, EVT_ACT_SEPARATE_BACK);
    break;
   case EVT_ACT_SEPARATE_SUCCESS:
       StateMachine_SendEvent(&axis->sm_act, EVT_ACT_SEPARATE_SUCCESS);
       EventGroup_Clear(axis->evt_act, EVT_ACT_SEPARATE_SUCCESS);
    break;
   case EVT_ACT_SEPARATE_FAIL:
       StateMachine_SendEvent(&axis->sm_act, EVT_ACT_SEPARATE_FAIL);
       EventGroup_Clear(axis->evt_act, EVT_ACT_SEPARATE_FAIL);
    break;
   case EVT_ACT_HOLD:
       StateMachine_SendEvent(&axis->sm_act, EVT_ACT_HOLD);
       EventGroup_Clear(axis->evt_act, EVT_ACT_HOLD);
    break;
   case EVT_ACT_ERROR:
       StateMachine_SendEvent(&axis->sm_act, EVT_ACT_ERROR);
       EventGroup_Clear(axis->evt_act, EVT_ACT_ERROR);
    break;
   case EVT_ACT_RESET:
       StateMachine_SendEvent(&axis->sm_act, EVT_ACT_RESET);
       EventGroup_Clear(axis->evt_act, EVT_ACT_RESET);
    break;
   // case EVT_ACT_STOP:
   //     StateMachine_SendEvent(&axis->sm_act, EVT_ACT_STOP);
   //     EventGroup_Clear(axis->evt_act, EVT_ACT_STOP);
   //  break;

   //260616_RL_add
      case EVT_ACT_CALIBRATION_ENTER:
       StateMachine_SendEvent(&axis->sm_act, EVT_ACT_CALIBRATION_ENTER);
       EventGroup_Clear(axis->evt_act, EVT_ACT_CALIBRATION_ENTER);
    break;
    case EVT_ACT_CALIBRATION_OUT:
       StateMachine_SendEvent(&axis->sm_act, EVT_ACT_CALIBRATION_OUT);
       EventGroup_Clear(axis->evt_act, EVT_ACT_CALIBRATION_OUT);
      break;
    case EVT_ACT_CALIBRATION_IN:
       StateMachine_SendEvent(&axis->sm_act, EVT_ACT_CALIBRATION_IN);
       EventGroup_Clear(axis->evt_act, EVT_ACT_CALIBRATION_IN);
    break;
    case EVT_ACT_CALIBRATION_STOP:
       StateMachine_SendEvent(&axis->sm_act, EVT_ACT_CALIBRATION_STOP);
       EventGroup_Clear(axis->evt_act, EVT_ACT_CALIBRATION_STOP);
    break;    
    case EVT_ACT_CALIBRATION_SUCCESS:
       StateMachine_SendEvent(&axis->sm_act, EVT_ACT_CALIBRATION_SUCCESS);
       EventGroup_Clear(axis->evt_act, EVT_ACT_CALIBRATION_SUCCESS);
    break;  
    case EVT_ACT_CALIBRATION_FALL:
       StateMachine_SendEvent(&axis->sm_act, EVT_ACT_CALIBRATION_FALL);
       EventGroup_Clear(axis->evt_act, EVT_ACT_CALIBRATION_FALL);
    break;      
   default:
    break;
   }
}

/* 状态入口函数 */
static void act_enter_idle(void)              {
    //260611_RL_add:
    for (uint8_t i = 0; i < MAX_AXIS_NUM; i++)
        if (mySystem.axis[i].enable)
        {            
          if (mySystem.axis[i].sm_act.cur_state == ACT_STATE_SEPARATE_SUCCESS ||
              mySystem.axis[i].sm_act.cur_state == ACT_STATE_SEPARATE_FAIL ||
              mySystem.axis[i].sm_act.cur_state == ACT_STATE_COMBINE_SUCCESS ||
              mySystem.axis[i].sm_act.cur_state == ACT_STATE_COMBINE_FAIL ||
              mySystem.axis[i].sm_act.cur_state == ACT_STATE_INIT ||
              mySystem.axis[i].sm_act.cur_state == ACT_STATE_IDLE  )
          {
             EventGroup_Send(mySystem.axis[i].motor.evt_mot, EVT_MOT_STOP);     //推杆停止
          }              
        }
}
static void act_enter_moving(void)            {
   
   for (uint8_t i = 0; i < MAX_AXIS_NUM; i++)
   {
      if (mySystem.axis[i].enable)
      {
         mySystem.axis[i].pos_ctrl.enable = true;
         mySystem.axis[i].spd_ctrl.enable = true;
         mySystem.axis[i].curr_ctrl.enable = true;

         if (mySystem.axis[i].sm_act.cur_state == ACT_STATE_COMBINE ||
            mySystem.axis[i].sm_act.cur_state == ACT_STATE_COMBINE_TRY_1 ||
            mySystem.axis[i].sm_act.cur_state == ACT_STATE_COMBINE_TRY_2 ||
            mySystem.axis[i].sm_act.cur_state == ACT_STATE_COMBINE_TRY_3 ||
            mySystem.axis[i].sm_act.cur_state == ACT_STATE_SEPARATE_BACK ||
            mySystem.axis[i].sm_act.cur_state == ACT_STATE_SEPARATE_BACK_1 ||
            mySystem.axis[i].sm_act.cur_state == ACT_STATE_SEPARATE_BACK_2 )
         {
            if (mySystem.axis[i].pos_current > mySystem.axis[i].pos_combine )
            {
               mySystem.axis[i].pos_ctrl.out_nor = -1.0f;
            }else{
               mySystem.axis[i].pos_ctrl.out_nor = 1.0f;
            }
            
            mySystem.axis[i].pos_ctrl.set = mySystem.axis[i].pos_combine;

            // 结合方向
            if (mySystem.axis[i].dir == ACT_DIR_COMBINE_IS_MOVE_OUT)
            {
               EventGroup_Send(mySystem.axis[i].motor.evt_mot, EVT_MOT_FORWARD);
            }else if (mySystem.axis[i].dir == ACT_DIR_COMBINE_IS_MOVE_IN)
            {
               EventGroup_Send(mySystem.axis[i].motor.evt_mot, EVT_MOT_REVERSE);
            }         
         }else if (mySystem.axis[i].sm_act.cur_state == ACT_STATE_SEPARATE ||
                  mySystem.axis[i].sm_act.cur_state == ACT_STATE_SEPARATE_TRY_1 ||
                  mySystem.axis[i].sm_act.cur_state == ACT_STATE_SEPARATE_TRY_2 ||
                  mySystem.axis[i].sm_act.cur_state == ACT_STATE_SEPARATE_TRY_3 ||
                  mySystem.axis[i].sm_act.cur_state == ACT_STATE_COMBINE_BACK   ||
                  mySystem.axis[i].sm_act.cur_state == ACT_STATE_COMBINE_BACK_1 ||
                  mySystem.axis[i].sm_act.cur_state == ACT_STATE_COMBINE_BACK_2 )
         {

            if (mySystem.axis[i].pos_current > mySystem.axis[i].pos_separate )
            {
               mySystem.axis[i].pos_ctrl.out_nor = -1.0f;
            }else{
               mySystem.axis[i].pos_ctrl.out_nor = 1.0f;
            }
            mySystem.axis[i].pos_ctrl.set = mySystem.axis[i].pos_separate;
            // 分离方向
            if (mySystem.axis[i].dir == ACT_DIR_COMBINE_IS_MOVE_OUT)
            {
               EventGroup_Send(mySystem.axis[i].motor.evt_mot, EVT_MOT_REVERSE);
            }else if (mySystem.axis[i].dir == ACT_DIR_COMBINE_IS_MOVE_IN)
            {
               EventGroup_Send(mySystem.axis[i].motor.evt_mot, EVT_MOT_FORWARD);
            }   
         }
      }
   }
}

static void act_enter_combine(void){
}
static void act_enter_separate(void)          {
}
	
static void act_enter_hold(void)              {

   for (uint8_t i = 0; i < MAX_AXIS_NUM; i++)
   {
      EventGroup_Send(mySystem.axis[i].motor.evt_mot, EVT_MOT_BRAKE);

      mySystem.axis[i].pos_ctrl.enable = false;
      mySystem.axis[i].spd_ctrl.enable = false;
      mySystem.axis[i].curr_ctrl.enable = false;
   }
}

/*
260611_RL_add:
堵转后调用函数*/
void act_babk_handler(uint8_t* MotorBackFlag)
{
    //2.1给推杆发送事件,根据推杆当前状态发送           
    //EventGroup_Send(mySystem.axis[i].evt_act, EVT_ACT_SEPARATE);
    //2.2 推杆回退需要计时1.2s
    //2.3 计时1.2s之后，发尝试事件
    //2.4 (几次尝试中接收到位置环的成功事件；否则报失败) 
    
   static uint32_t s_BackStartTime[MAX_AXIS_NUM] = {0};
    
   for (uint8_t i = 0; i < MAX_AXIS_NUM; i++)
   {
      if (mySystem.axis[i].sm_act.cur_state == ACT_STATE_SEPARATE ||
          mySystem.axis[i].sm_act.cur_state == ACT_STATE_SEPARATE_TRY_1 ||
          mySystem.axis[i].sm_act.cur_state == ACT_STATE_SEPARATE_TRY_2 ||
          mySystem.axis[i].sm_act.cur_state == ACT_STATE_SEPARATE_TRY_3 )      //每次回退都要有1.2s的限时。
      {
          if(mySystem.axis[i].sm_act.cur_state == ACT_STATE_SEPARATE_TRY_3)
          {
            //发送堵转，分离失败,不用计时
            EventGroup_Send(mySystem.axis[i].evt_act, EVT_ACT_SEPARATE_BACK);
            //清除堵转标志
            *MotorBackFlag = 0;               
          }
          else
          {
              //发送分离遇阻事件 EVT_ACT_SEPARATE_BACK
              EventGroup_Send(mySystem.axis[i].evt_act, EVT_ACT_SEPARATE_BACK);
              //开始计时
              s_BackStartTime[i] = SysTick_GetTick();
          }
      }
      else if(mySystem.axis[i].sm_act.cur_state == ACT_STATE_COMBINE   ||
              mySystem.axis[i].sm_act.cur_state == ACT_STATE_COMBINE_TRY_1 ||
              mySystem.axis[i].sm_act.cur_state == ACT_STATE_COMBINE_TRY_2  ||
              mySystem.axis[i].sm_act.cur_state == ACT_STATE_COMBINE_TRY_3)       //每次回退都要有1.2s的限时。
      {
        if(mySystem.axis[i].sm_act.cur_state == ACT_STATE_COMBINE_TRY_3)
        {
            //发送堵转事件，直接跳到失败，不需要计时
            EventGroup_Send(mySystem.axis[i].evt_act, EVT_ACT_COMBINE_BACK); 
            //清除堵转标志
            *MotorBackFlag = 0; 
        }
        else
        {          
          //发送结合遇阻事件 EVT_ACT_COMBINE_BACK/失败
          EventGroup_Send(mySystem.axis[i].evt_act, EVT_ACT_COMBINE_BACK);
          //开始计时
          s_BackStartTime[i] = SysTick_GetTick();
        }
      }
           
      if (mySystem.axis[i].sm_act.cur_state == ACT_STATE_SEPARATE_BACK ||
          mySystem.axis[i].sm_act.cur_state == ACT_STATE_SEPARATE_BACK_1 ||
          mySystem.axis[i].sm_act.cur_state == ACT_STATE_SEPARATE_BACK_2 )     
      {
          //计时到了以后发再次尝试分离
        uint32_t NowTime = SysTick_GetTick();
        if((NowTime - s_BackStartTime[i]) > 1200)  //超过1.2  
        {
            //根据当前状态发再次分离
            EventGroup_Send(mySystem.axis[i].evt_act, EVT_ACT_SEPARATE);
           //清除堵转标志
            *MotorBackFlag = 0; 
        }         
         
      }
      else if(mySystem.axis[i].sm_act.cur_state == ACT_STATE_COMBINE_BACK   ||
              mySystem.axis[i].sm_act.cur_state == ACT_STATE_COMBINE_BACK_1 ||
              mySystem.axis[i].sm_act.cur_state == ACT_STATE_COMBINE_BACK_2 )
      {
        //计时到了以后发再次尝试结合
        uint32_t NowTime = SysTick_GetTick();
        if((NowTime - s_BackStartTime[i]) > 1200)  //超过1.2  
        {
            //根据当前状态发再次分离
            EventGroup_Send(mySystem.axis[i].evt_act, EVT_ACT_COMBINE);
            //清除堵转标志
            *MotorBackFlag = 0; 
        }         
      }
  }
    
}

//260616_RL_add: 标定过程执行函数
static void act_enter_calibration(void)
{
    for (uint8_t i = 0; i < MAX_AXIS_NUM; i++)
    {
      if(mySystem.axis[i].sm_act.cur_state == ACT_STATE_CALIBRATION )     
        {
            EventGroup_Send(mySystem.axis[i].motor.evt_mot, EVT_MOT_STOP);     //电机停止 
        }
        else if(mySystem.axis[i].sm_act.cur_state == ACT_STATE_CALIBRATION_OUT)
        {
            EventGroup_Send(mySystem.axis[i].motor.evt_mot, EVT_MOT_FORWARD);   //电机直接伸出，不判断推杆的方向
        }
        else if(mySystem.axis[i].sm_act.cur_state == ACT_STATE_CALIBRATION_IN)
        {
            EventGroup_Send(mySystem.axis[i].motor.evt_mot, EVT_MOT_REVERSE);   //电机缩回
        }      
  }
    
}	
	
	
