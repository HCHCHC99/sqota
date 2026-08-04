
#include "key.h"
#include "event_def.h"
#include "hc32_ll_gpio.h"

#include "msg_pubsub.h"
#include "msg_topics.h"
// #include "key.h"
 #include "event_group.h"
// #include "event_def.h"
// #include "sys_tick.h"
// #include "hc32_ll_gpio.h"
// #include "hc32_ll_utility.h"

/* 外部全局事件组 */
EventGroup_t g_sys_event_group;

/* 外部硬件配置表 */
extern const Key_HwConfig_t KEY_HW_TABLE[KEY_MAX_COUNT];

/* 组合按键映射表（虚拟按键） */
static const Key_Combine_Map_t KEY_COMBINE_MAP[] = {
    /* KEY0 + KEY1 长按 → 虚拟按键 */
    { {0, 1}, 2, 200, 0x20, "KEY0+KEY1_LONG" },
};

#define COMBINE_MAP_CNT  (sizeof(KEY_COMBINE_MAP)/sizeof(Key_Combine_Map_t))

/*****************************************
 * 内部状态机枚举
 ****************************************/
typedef enum {
    KEY_ST_IDLE = 0U,        /* 空闲状态 */
    KEY_ST_DEBOUNCE,         /* 消抖状态 */
    KEY_ST_PRESSED,          /* 短按确认状态 */
    KEY_ST_LONG_PRESS,       /* 长按状态 */
} Key_State_t;

typedef enum {
    KEY_SM_EVT_PRESS = 0U,
    KEY_SM_EVT_RELEASE,
    KEY_SM_EVT_LONG_TIMEOUT,
} Key_Sm_Event_t;

/*****************************************
 * 内部全局变量
 ****************************************/
static uint8_t      key_scan_buf[KEY_MAX_COUNT][KEY_SCAN_BUFFER_LEN];
static uint8_t      key_scan_idx[KEY_MAX_COUNT];
static Key_Object_t g_key[KEY_MAX_COUNT];

/*****************************************
 * 状态机回调函数
 ****************************************/
static void key_idle_enter(void)         { }
static void key_debounce_enter(void)     { }
static void key_pressed_enter(void)      { }
static void key_long_press_enter(void)   { }

/*****************************************
 * 状态机跳转表
 ****************************************/
static const StateJumpTable_t key_jump_table[] = {
    {KEY_ST_IDLE,        KEY_SM_EVT_PRESS,      KEY_ST_PRESSED},
    // {KEY_ST_DEBOUNCE,    KEY_SM_EVT_RELEASE,    KEY_ST_IDLE},    // 单独处理了 抖动，无需在状态机添加 抖动状态
    // {KEY_ST_DEBOUNCE,    KEY_SM_EVT_PRESS,      KEY_ST_PRESSED},
    {KEY_ST_PRESSED,     KEY_SM_EVT_RELEASE,    KEY_ST_IDLE},
    {KEY_ST_PRESSED,     KEY_SM_EVT_LONG_TIMEOUT, KEY_ST_LONG_PRESS},
    {KEY_ST_LONG_PRESS,  KEY_SM_EVT_RELEASE,    KEY_ST_IDLE},
};

static const StateFuncTable_t key_func_table[] = {
    {KEY_ST_IDLE,        key_idle_enter,        NULL},
    {KEY_ST_DEBOUNCE,    key_debounce_enter,    NULL},
    {KEY_ST_PRESSED,     key_pressed_enter,     NULL},
    {KEY_ST_LONG_PRESS,  key_long_press_enter,  NULL},
};

/*****************************************
 * @brief  获取按键稳定状态
 * @param  key: 按键对象指针
 * @retval 1-有效按下 0-未按下
 ****************************************/
static uint8_t Key_GetStableState(Key_Object_t *key)
{
    uint8_t act = key->hw->active_level;
    uint8_t cnt = 0U;

    for (uint8_t i = 0U; i < KEY_SCAN_BUFFER_LEN; i++)
    {
        if (key_scan_buf[key->key_id][i] == act)
        {
            cnt++;
        }
    }
    return (cnt >= KEY_SCAN_BUFFER_LEN) ? 1U : 0U;
}

/*****************************************
 * @brief  检查组合按键是否全部按下
 ****************************************/
static bool Key_CheckCombineAllPressed(const Key_Combine_Map_t *map)
{
    for (uint8_t i = 0U; i < map->key_num; i++)
    {
        uint8_t idx = map->key_index[i];
        if (idx >= KEY_MAX_COUNT) return false;
        if (!Key_GetStableState(&g_key[idx])) return false;
    }
    return true;
}

/*****************************************
 * @brief  按键GPIO初始化
 ****************************************/
static void Key_GPIO_Init(const Key_HwConfig_t *hw)
{
    stc_gpio_init_t stcGpioCfg;
    GPIO_StructInit(&stcGpioCfg);

    stcGpioCfg.u16PinDir = PIN_DIR_IN;

    /* 低电平有效 → 上拉 */
    if (hw->active_level == 0U)
    {
        stcGpioCfg.u16PullUp = PIN_PU_ON;
    }

    GPIO_Init(hw->gpio, hw->pin, &stcGpioCfg);
}

/*****************************************
 * @brief  全局按键初始化
 ****************************************/
void Key_Init(void)
{
    for (uint8_t i = 0U; i < KEY_MAX_COUNT; i++)
    {
        Key_Object_t *key = &g_key[i];
        key->key_id = i;
        key->hw = &KEY_HW_TABLE[i];
        key->locked = 0U;
        key->long_timer = 0U;
        key->stable_now = 0U;
        key->stable_last = 0U;

        /* GPIO初始化 */
        Key_GPIO_Init(key->hw);

        /* 状态机初始化 */
        key->sm.jump_table = key_jump_table;
        key->sm.jump_table_size = sizeof(key_jump_table)/sizeof(StateJumpTable_t);
        key->sm.func_table = key_func_table;
        key->sm.func_table_size = sizeof(key_func_table)/sizeof(StateFuncTable_t);
        key->sm.init_state = KEY_ST_IDLE;
        StateMachine_Init(&key->sm);

        /* 消抖缓冲区初始化 */
        for (uint8_t j = 0U; j < KEY_SCAN_BUFFER_LEN; j++)
        {
            key_scan_buf[i][j] = !key->hw->active_level;
        }
        key_scan_idx[i] = 0U;
    }
}

/*****************************************
 * @brief  统一上报键盘事件（物理键 + 虚拟键）
 ****************************************/
static void Key_SendReport(uint32_t code, const char *name, Key_Event_t evt)
{
    Key_Report_t report = {0};
    report.key_code = code;
    report.key_name = name;
    report.event = evt;
    report.timestamp = SysTick_GetTick();

    /* 通过 发布订阅机制 发送到主题 */
    Msg_Publish(TOPIC_KEYS_STATE, &report, sizeof(report), MSG_PRIO_LOW);
}

/*****************************************
 * @brief  1ms 按键任务
 ****************************************/
void Key_Task(void)
{
    uint32_t tick = SysTick_GetTick();
    uint8_t  combo_activated = 0U;

    /* ===================== 1. GPIO电平采样 ===================== */
    for (uint8_t i = 0U; i < KEY_MAX_COUNT; i++)
    {
        uint8_t val = GPIO_ReadInputPins(g_key[i].hw->gpio, g_key[i].hw->pin);
        key_scan_buf[i][key_scan_idx[i]] = val;
        key_scan_idx[i] = (key_scan_idx[i] + 1U) % KEY_SCAN_BUFFER_LEN;
    }

    /* ===================== 2. 组合按键检测（虚拟按键） ===================== */
    static uint32_t combo_timer[COMBINE_MAP_CNT] = {0};
    for (uint8_t i = 0U; i < COMBINE_MAP_CNT; i++)
    {
        const Key_Combine_Map_t *map = &KEY_COMBINE_MAP[i];
        if (Key_CheckCombineAllPressed(map))
        {
            combo_activated = 1U;
            if (combo_timer[i] < map->long_ms)
            {
                combo_timer[i]++;
            }
            else
            {
                /* 组合键 → 统一虚拟按键上报，无特殊事件 */
                Key_SendReport(map->virtual_code, map->virtual_name, KEY_EVENT_LONG);
                combo_timer[i] = 0U;
            }
        }
        else
        {
            combo_timer[i] = 0U;
        }
    }

    /* ===================== 3. 单按键状态机处理 ===================== */
    for (uint8_t i = 0U; i < KEY_MAX_COUNT; i++)
    {
        Key_Object_t *key = &g_key[i];
        key->stable_now = Key_GetStableState(key);

        /* 组合键激活时锁定单键 */
        if (combo_activated)
        {
            key->locked = 1U;
        }
        else
        {
            key->locked = 0U;
        }

        /* 边沿检测 */
        if (key->stable_now != key->stable_last)
        {
            key->stable_last = key->stable_now;

            if (key->stable_now)
            {
                /* 按下 */
                key->tick_press = tick;
                key->long_timer = 0U;
                StateMachine_SendEvent(&key->sm, KEY_SM_EVT_PRESS);
                
            }
            else
            {
                /* 释放 */
                key->tick_release = tick;
                StateMachine_SendEvent(&key->sm, KEY_SM_EVT_RELEASE);

                /* 单击判定 */
                if (!key->locked)
                {
                    uint32_t press_time = key->tick_release - key->tick_press;
                    if (press_time < key->hw->long_press_ms)
                    {
                        Key_SendReport(i, (const char*)&i, KEY_EVENT_CLICK);
                    }
                }

                /* 释放事件 */
                Key_SendReport(i, (const char*)&i, KEY_EVENT_RELEASE);
            }
        }

        /* 长按逻辑 */
        if (key->sm.cur_state == KEY_ST_PRESSED)
        {
            if (key->stable_now && !key->locked)
            {
                key->long_timer++;
                if (key->long_timer >= key->hw->long_press_ms)
                {
                    StateMachine_SendEvent(&key->sm, KEY_SM_EVT_LONG_TIMEOUT);
                    key->long_timer = 0U;
                    Key_SendReport(i, (const char*)&i, KEY_EVENT_LONG);
                }
            }
        }
    }
}



