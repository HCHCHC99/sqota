#include "soft_timer.h"
#include "hw_tim_driver.h"  // 底层硬件定时器驱动头文件，芯片相关

#ifndef NULL
  #define NULL                                            0
#endif

/* 静态实例池，全部静态分配，无堆内存 */
static SoftTimer_HandleDef m_soft_timer_pool[SOFT_TIMER_MAX_NUM] = {0};
static bool m_module_init_flag = false;

/**
 * @brief 全局初始化：绑定硬件定时器
 */
void SoftTimer_GlobalInit(void)
{
    if(m_module_init_flag)
    {
        return;
    }
    // 调用底层硬件驱动初始化指定TIM，配置中断周期SOFT_TIMER_TICK_MS
    if(HW_TIM_Init(HW_TIM_INDEX, SOFT_TIMER_TICK_MS) != true)
    {
        return;
    }
    m_module_init_flag = true;
}

/**
 * @brief 查找空闲定时器实例，返回句柄
 */
SoftTimer_Handle_t SoftTimer_Register(void)
{
    if(!m_module_init_flag)
    {
        return NULL;
    }
    for(uint8_t i = 0; i < SOFT_TIMER_MAX_NUM; i++)
    {
        if(m_soft_timer_pool[i].is_register == false)
        {
            // 初始化该实例默认参数
            m_soft_timer_pool[i].is_register = true;
            m_soft_timer_pool[i].is_running  = false;
            m_soft_timer_pool[i].mode        = SOFT_TIMER_MODE_ONCE;
            m_soft_timer_pool[i].period_ms   = 0;
            m_soft_timer_pool[i].tick_cnt    = 0;
            m_soft_timer_pool[i].callback    = NULL;
            return &m_soft_timer_pool[i];
        }
    }
    // 无空闲实例
    return NULL;
}

/**
 * @brief 注销定时器
 */
void SoftTimer_UnRegister(SoftTimer_Handle_t htimer)
{
    if(htimer == NULL) return;
    htimer->is_register = false;
    htimer->is_running  = false;
    htimer->callback    = NULL;
}

/**
 * @brief 绑定回调
 */
void SoftTimer_SetCallback(SoftTimer_Handle_t htimer, SoftTimer_Callback_t cb)
{
    if(htimer == NULL) return;
    htimer->callback = cb;
}

/**
 * @brief 启动定时器
 */
void SoftTimer_Start(SoftTimer_Handle_t htimer, SoftTimer_Mode_t mode, uint32_t time_ms)
{
    if(htimer == NULL || !htimer->is_register || time_ms == 0)
    {
        return;
    }
    htimer->mode       = mode;
    htimer->period_ms  = time_ms;
    htimer->tick_cnt    = 0;
    htimer->is_running = true;
}

/**
 * @brief 停止定时器
 */
void SoftTimer_Stop(SoftTimer_Handle_t htimer)
{
    if(htimer == NULL) return;
    htimer->is_running = false;
    htimer->tick_cnt   = 0;
}

/**
 * @brief 硬件定时器中断滴答调度核心
 * 每进一次硬件TIM中断，调用此函数遍历所有软定时器计数
 */
void SoftTimer_TickISR(void)
{
    uint32_t tick_step = SOFT_TIMER_TICK_MS;

    for(uint8_t i = 0; i < SOFT_TIMER_MAX_NUM; i++)
    {
        SoftTimer_Handle_t htimer = &m_soft_timer_pool[i];
        // 仅处理已注册且正在运行的定时器
        if(!htimer->is_register || !htimer->is_running)
        {
            continue;
        }
        htimer->tick_cnt += tick_step;
        // 判断是否到达定时时间
        if(htimer->tick_cnt >= htimer->period_ms)
        {
            // 执行回调（回调尽量简短，禁止阻塞、长延时）
            if(htimer->callback != NULL)
            {
                htimer->callback(htimer);
            }
            // 根据模式处理
            if(htimer->mode == SOFT_TIMER_MODE_ONCE)
            {
                // 单次模式：停止
                htimer->is_running = false;
                htimer->tick_cnt   = 0;
            }
            else
            {
                // 周期模式：重置计数，循环运行
                htimer->tick_cnt = 0;
            }
        }
    }
}
