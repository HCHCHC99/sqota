
#include "led.h"
#include "event_group.h"
#include "state_engine.h"
#include "sys_tick.h"
#include "msg_topics.h"
#include "msg_pubsub.h"

#include "hc32_ll_utility.h"
#include "hc32_ll_gpio.h"

#include "key.h"

#define LED_COUNT     2   

// 全局LED对象数组（自动根据LED_COUNT扩展）
static LED_Object_t g_led[LED_COUNT];

const char *event_group_led_names[] = {
    "led1",
    "led2"
};


// 频率表：快/中/慢  10ms 基准
static const uint16_t freq_table[][2] = {
    {LED_FREQ1_ON, LED_FREQ1_OFF},    // LED_FREQ_1
    {LED_FREQ2_ON, LED_FREQ2_OFF},    // LED_FREQ_2
    {LED_FREQ3_ON, LED_FREQ3_OFF},    // LED_FREQ_3
    {LED_FREQ4_ON, LED_FREQ4_OFF},    // LED_FREQ_4 //260617_RL_add
};

// 内部函数声明
static void led_off_enter(void);
static void led_on_enter(void);
static void led_blink_enter(void);
static void led_blink_process(LED_Object_t *led);
static void led_set_color(LED_Object_t *led, LED_Color_t color);
static void LED_SendActiveEvent(LED_Object_t *led, LED_Event_t evt);

// 状态机跳转表（简化：统一ON状态，支持任意颜色）
static const StateJumpTable_t led_jump_table[] = {

    {LED_ST_DISABLE, LED_EVT_ENABLE,    LED_ST_OFF},            //260613_RL_add

    {LED_ST_OFF,    LED_EVT_OFF,        LED_ST_OFF},
    {LED_ST_OFF,    LED_EVT_RED_ON,     LED_ST_RED_ON},
    {LED_ST_OFF,    LED_EVT_GREEN_ON,   LED_ST_GREEN_ON},
    {LED_ST_OFF,    LED_EVT_YELLOW_ON,  LED_ST_YELLOW_ON},
    {LED_ST_OFF,    LED_EVT_RED_YELLOW_ON,  LED_ST_RED_YELLOW_ON},
    {LED_ST_OFF,    LED_EVT_COLOR_ALL_ON,  LED_ST_COLOR_ALL_ON},
		
    {LED_ST_OFF,    LED_EVT_BLINK_1,      LED_ST_BLINK_1},
    {LED_ST_OFF,    LED_EVT_BLINK_2,      LED_ST_BLINK_2},
    {LED_ST_OFF,    LED_EVT_BLINK_3,      LED_ST_BLINK_3},
    {LED_ST_OFF,    LED_EVT_BLINK_4,      LED_ST_BLINK_4},
    {LED_ST_OFF,    LED_EVT_DISABLE,      LED_ST_DISABLE},       //260613_RL_add

    {LED_ST_RED_ON,  LED_EVT_OFF,       LED_ST_OFF},
//    {LED_ST_RED_ON,  LED_EVT_BLINK,     LED_ST_BLINK},
    {LED_ST_RED_ON,    LED_EVT_BLINK_1,      LED_ST_BLINK_1},
    {LED_ST_RED_ON,    LED_EVT_BLINK_2,      LED_ST_BLINK_2},
    {LED_ST_RED_ON,    LED_EVT_BLINK_3,      LED_ST_BLINK_3},
    {LED_ST_RED_ON,    LED_EVT_BLINK_4,      LED_ST_BLINK_4},
    {LED_ST_RED_ON,    LED_EVT_DISABLE,      LED_ST_DISABLE},    //260613_RL_add
		
    {LED_ST_GREEN_ON,LED_EVT_OFF,       LED_ST_OFF},
//    {LED_ST_GREEN_ON,LED_EVT_BLINK,     LED_ST_BLINK},
    {LED_ST_GREEN_ON,    LED_EVT_BLINK_1,      LED_ST_BLINK_1},
    {LED_ST_GREEN_ON,    LED_EVT_BLINK_2,      LED_ST_BLINK_2},
    {LED_ST_GREEN_ON,    LED_EVT_BLINK_3,      LED_ST_BLINK_3},
    {LED_ST_GREEN_ON,    LED_EVT_BLINK_4,      LED_ST_BLINK_4},
    {LED_ST_GREEN_ON,    LED_EVT_DISABLE,      LED_ST_DISABLE},    //260613_RL_add
		
    {LED_ST_YELLOW_ON,LED_EVT_OFF,      LED_ST_OFF},
//    {LED_ST_YELLOW_ON,LED_EVT_BLINK,    LED_ST_BLINK},
    {LED_ST_YELLOW_ON,    LED_EVT_BLINK_1,      LED_ST_BLINK_1},
    {LED_ST_YELLOW_ON,    LED_EVT_BLINK_2,      LED_ST_BLINK_2},
    {LED_ST_YELLOW_ON,    LED_EVT_BLINK_3,      LED_ST_BLINK_3},
    {LED_ST_YELLOW_ON,    LED_EVT_BLINK_4,      LED_ST_BLINK_4},
    {LED_ST_YELLOW_ON,    LED_EVT_DISABLE,      LED_ST_DISABLE},    //260613_RL_add

    {LED_ST_RED_YELLOW_ON,LED_EVT_OFF,      LED_ST_OFF},
//    {LED_ST_YELLOW_ON,LED_EVT_BLINK,    LED_ST_BLINK},
    {LED_ST_RED_YELLOW_ON,    LED_EVT_BLINK_1,      LED_ST_BLINK_1},
    {LED_ST_RED_YELLOW_ON,    LED_EVT_BLINK_2,      LED_ST_BLINK_2},
    {LED_ST_RED_YELLOW_ON,    LED_EVT_BLINK_3,      LED_ST_BLINK_3},
    {LED_ST_RED_YELLOW_ON,    LED_EVT_BLINK_4,      LED_ST_BLINK_4},
    {LED_ST_RED_YELLOW_ON,    LED_EVT_DISABLE,      LED_ST_DISABLE},    //260613_RL_add

    {LED_ST_COLOR_ALL_ON,  LED_EVT_BLINK_1,        LED_ST_BLINK_1},
    {LED_ST_COLOR_ALL_ON,  LED_EVT_BLINK_2,        LED_ST_BLINK_2},
    {LED_ST_COLOR_ALL_ON,  LED_EVT_BLINK_3,        LED_ST_BLINK_3},
    {LED_ST_COLOR_ALL_ON,  LED_EVT_BLINK_4,        LED_ST_BLINK_4},
    {LED_ST_COLOR_ALL_ON,  LED_EVT_DISABLE,        LED_ST_DISABLE},    //260613_RL_add

    {LED_ST_BLINK_1,  LED_EVT_BLINK_2,        LED_ST_BLINK_2},
    {LED_ST_BLINK_1,  LED_EVT_BLINK_3,        LED_ST_BLINK_3},
    {LED_ST_BLINK_1,  LED_EVT_BLINK_4,        LED_ST_BLINK_4},
    {LED_ST_BLINK_1,  LED_EVT_DISABLE,        LED_ST_DISABLE},    //260613_RL_add
    
    {LED_ST_BLINK_2,  LED_EVT_BLINK_1,        LED_ST_BLINK_1},
    {LED_ST_BLINK_2,  LED_EVT_BLINK_3,        LED_ST_BLINK_3},
    {LED_ST_BLINK_2,  LED_EVT_BLINK_4,        LED_ST_BLINK_4},
    {LED_ST_BLINK_2,  LED_EVT_DISABLE,        LED_ST_DISABLE},    //260613_RL_add

    {LED_ST_BLINK_3,  LED_EVT_BLINK_1,        LED_ST_BLINK_1},
    {LED_ST_BLINK_3,  LED_EVT_BLINK_2,        LED_ST_BLINK_2},
    {LED_ST_BLINK_3,  LED_EVT_BLINK_4,        LED_ST_BLINK_4},
    {LED_ST_BLINK_3,  LED_EVT_DISABLE,        LED_ST_DISABLE},    //260613_RL_add

    {LED_ST_BLINK_4,  LED_EVT_BLINK_1,        LED_ST_BLINK_1},
    {LED_ST_BLINK_4,  LED_EVT_BLINK_2,        LED_ST_BLINK_2},
    {LED_ST_BLINK_4,  LED_EVT_BLINK_3,        LED_ST_BLINK_3},
    {LED_ST_BLINK_4,  LED_EVT_DISABLE,        LED_ST_DISABLE},    //260613_RL_add

		
    {LED_ST_BLINK_1,  LED_EVT_OFF,        LED_ST_OFF},
    {LED_ST_BLINK_2,  LED_EVT_OFF,        LED_ST_OFF},
    {LED_ST_BLINK_3,  LED_EVT_OFF,        LED_ST_OFF},
    {LED_ST_BLINK_4,  LED_EVT_OFF,        LED_ST_OFF},
};

// 状态机函数表
static const StateFuncTable_t led_func_table[] = {
    {LED_ST_OFF,            led_off_enter,     NULL},
    {LED_ST_RED_ON,         led_on_enter,      NULL},
    {LED_ST_GREEN_ON,       led_on_enter,      NULL},
    {LED_ST_YELLOW_ON,      led_on_enter,      NULL},
    {LED_ST_RED_YELLOW_ON,  led_on_enter,      NULL},
    {LED_ST_COLOR_ALL_ON,   led_on_enter,      NULL},
    {LED_ST_BLINK_1,        led_blink_enter,   NULL},
    {LED_ST_BLINK_2,        led_blink_enter,   NULL},
    {LED_ST_BLINK_3,        led_blink_enter,   NULL},
	{LED_ST_BLINK_4,        led_blink_enter,   NULL},
    {LED_ST_DISABLE,        led_off_enter,     NULL},       //260613_RL_add
		
};

// 任意颜色输出（单色+组合色）
static void led_set_color(LED_Object_t *led, LED_Color_t color) {
		
    en_pin_state_t act = (en_pin_state_t)(led->hw->active_level);
	en_pin_state_t r = (act == PIN_SET) ?  PIN_RESET : PIN_SET;
    en_pin_state_t g = (act == PIN_SET) ?  PIN_RESET : PIN_SET;
    en_pin_state_t y = (act == PIN_SET) ?  PIN_RESET : PIN_SET;

    switch(color) {
        case LED_COLOR_RED:          r = act; break;
        case LED_COLOR_GREEN:        g = act; break;
        case LED_COLOR_YELLOW:       y = act; break;
        case LED_COLOR_RED_GREEN:    r = act; g = act; break;
        case LED_COLOR_RED_YELLOW:   r = act; y = act; break;
        case LED_COLOR_GREEN_YELLOW: g = act; y = act; break;
        case LED_COLOR_ALL:          r = act; g = act; y = act; break;
        case LED_COLOR_OFF:
        default:                     break;
    }

    (r == PIN_SET) ? GPIO_SetPins(led->hw->gpio_r, led->hw->pin_r) : GPIO_ResetPins(led->hw->gpio_r, led->hw->pin_r);
    (g == PIN_SET) ? GPIO_SetPins(led->hw->gpio_g, led->hw->pin_g) : GPIO_ResetPins(led->hw->gpio_g, led->hw->pin_g);
    (y == PIN_SET) ? GPIO_SetPins(led->hw->gpio_y, led->hw->pin_y) : GPIO_ResetPins(led->hw->gpio_y, led->hw->pin_y);
		
}

/**
 * @brief key状态 - key信息回调处理函数
 *
 * @param topic  消息主题ID（用于区分不同类型的消息）
 * @param data   指向位置信息数据缓冲区的指针
 * @param len    数据缓冲区长度（单位：字节）
 * @param prio   消息优先级
 *
 * @note 该函数用于接收并处理 key发布的按键数据
 */
void Led_Key_Callback(uint32_t topic, const void* data, uint16_t len, uint8_t prio);

void LED_Init(void) {
    for (uint8_t i = 0; i < LED_COUNT; i++) {
        LED_Object_t *led = &g_led[i];
        led->hw = &LED_HW_TABLE[i];
        led->color = LED_COLOR_OFF;
        led->alt_color_1 = LED_COLOR_RED;
        led->alt_color_2 = LED_COLOR_GREEN;
        led->freq = LED_FREQ_2;
        led->blink_mode = LED_BLINK_MODE_LOOP;

        led->pin_state = 0;
        led->target_cnt = 0;
        led->cur_cnt = 0;
        led->target_time = 0;
        led->cur_time = 0;

        led->task_jiffies = 0;
        led->blink_jiffies = 0;
        led->phase_jiffies = 0;
        led->interval_jiffies = 500;

        // 状态机绑定
        led->sm.jump_table = led_jump_table;
        led->sm.jump_table_size = sizeof(led_jump_table)/sizeof(StateJumpTable_t);
        led->sm.func_table = led_func_table;
        led->sm.func_table_size = sizeof(led_func_table)/sizeof(StateFuncTable_t);
        led->sm.init_state = LED_ST_DISABLE;        //260613_RL_fix:led初始化后进入失能状态
        StateMachine_Init(&led->sm);

        /*创建事件组*/
        led->evt_led = EventGroup_Create(event_group_led_names[i]);
    }

    // GPIO 初始化
    stc_gpio_init_t stcGpioInit;
    (void)GPIO_StructInit(&stcGpioInit);
    stcGpioInit.u16PinDir = PIN_DIR_OUT;

    for (uint8_t i = 0; i < LED_COUNT; i++) {
        LED_Object_t *led = &g_led[i];
        if (led->hw->active_level) {
            stcGpioInit.u16PullUp = PIN_STAT_RST;
        } else {
            stcGpioInit.u16PullUp = PIN_STAT_SET;
        }
        GPIO_Init(led->hw->gpio_g, led->hw->pin_g, &stcGpioInit);
        GPIO_Init(led->hw->gpio_r, led->hw->pin_r, &stcGpioInit);
        GPIO_Init(led->hw->gpio_y, led->hw->pin_y, &stcGpioInit);
    }

    // 订阅来自 key模块的 数据 （按键值、按键事件、事件触发时间戳）
    Msg_Subscribe(TOPIC_KEYS_STATE, Led_Key_Callback);
}

// 统一事件处理：每个LED对应独立事件组
static void LED_ProcessEvent(uint8_t led_idx, LED_Object_t *led, EventGroup_t *evt_group)
{
    uint32_t evt = evt_group->event_bits;
	
	  // 没有事件 → 直接退出
    if (evt == 0) return;

    //使能    260613_RL_add
    if (evt & EVT_LEDx_ENABLE) {
        StateMachine_SendEvent(&led->sm, LED_EVT_ENABLE);
    }
    //失能     260613_RL_add
    if (evt & EVT_LEDx_DISABLE) {
        StateMachine_SendEvent(&led->sm, LED_EVT_DISABLE);
    }    

    // 关闭
    if (evt & EVT_LEDx_OFF) {
        StateMachine_SendEvent(&led->sm, LED_EVT_OFF);
    }
    // 颜色设置
    if (evt & EVT_LEDx_RED_ON)  { 
		led->color = LED_COLOR_RED;           
		// StateMachine_SendEvent(&led->sm, LED_EVT_RED_ON); 
        LED_SendActiveEvent(led, LED_EVT_RED_ON);    //260617_RL_add
		}
    if (evt & EVT_LEDx_GREEN_ON)  { 
        led->color = LED_COLOR_GREEN;         
        // StateMachine_SendEvent(&led->sm, LED_EVT_GREEN_ON); 
        LED_SendActiveEvent(led, LED_EVT_GREEN_ON);     //260617_RL_add   
    }
    if (evt & EVT_LEDx_YELLOW_ON)  { 
        led->color = LED_COLOR_YELLOW;        
        // StateMachine_SendEvent(&led->sm, LED_EVT_YELLOW_ON); 
        LED_SendActiveEvent(led, LED_EVT_YELLOW_ON);     //260617_RL_add
    }
    if (evt & EVT_LEDx_RED_GREEN_ON)  { 
        led->color = LED_COLOR_RED_GREEN;     
        // StateMachine_SendEvent(&led->sm, LED_EVT_RED_ON);
        LED_SendActiveEvent(led, LED_EVT_RED_ON);       //260617_RL_add
    }
    if (evt & EVT_LEDx_RED_YELLOW_ON)  { 
        led->color = LED_COLOR_RED_YELLOW;    
        // StateMachine_SendEvent(&led->sm, LED_EVT_RED_ON); 
        LED_SendActiveEvent(led, LED_EVT_RED_ON);      //260617_RL_add 
    }
    if (evt & EVT_LEDx_GREEN_YELLOW_ON)  { 
        led->color = LED_COLOR_GREEN_YELLOW;  
        // StateMachine_SendEvent(&led->sm, LED_EVT_RED_ON); 
        LED_SendActiveEvent(led, LED_EVT_RED_ON);       //260617_RL_add  
    }
    if (evt & EVT_LEDx_ALL_ON)  { 
        led->color = LED_COLOR_ALL;          
        // StateMachine_SendEvent(&led->sm, LED_EVT_RED_ON); 
        LED_SendActiveEvent(led, LED_EVT_RED_ON);       //260617_RL_add
    }

    // 闪烁模式
    if (evt & EVT_LEDx_BLINK_LOOP)  { 
        led->blink_mode = LED_BLINK_MODE_LOOP; 
        // StateMachine_SendEvent(&led->sm, LED_EVT_BLINK_1); 
        LED_SendActiveEvent(led, LED_EVT_BLINK_1);    //260617_RL_add 
    }
    if (evt & EVT_LEDx_BLINK_CNT_3)  { 
        led->blink_mode = LED_BLINK_MODE_COUNT; 
        led->target_cnt = 3; 
        // StateMachine_SendEvent(&led->sm, LED_EVT_BLINK_1); 
        LED_SendActiveEvent(led, LED_EVT_BLINK_1);    //260617_RL_add
    }
    if (evt & EVT_LEDx_BLINK_CNT_5) { 
        led->blink_mode = LED_BLINK_MODE_COUNT; 
        led->target_cnt = 5; 
        // StateMachine_SendEvent(&led->sm, LED_EVT_BLINK_1); 
        LED_SendActiveEvent(led, LED_EVT_BLINK_1);    //260617_RL_add 
    }
    if (evt & EVT_LEDx_BLINK_TIME_2S) { 
        led->blink_mode = LED_BLINK_MODE_TIME; 
        led->target_time = 200; 
        // StateMachine_SendEvent(&led->sm, LED_EVT_BLINK_1); 
        LED_SendActiveEvent(led, LED_EVT_BLINK_1);   //260617_RL_add      
    }
    if (evt & EVT_LEDx_BLINK_TIME_5S) { 
        led->blink_mode = LED_BLINK_MODE_TIME; 
        led->target_time = 500; 
        // StateMachine_SendEvent(&led->sm, LED_EVT_BLINK_1); 
        LED_SendActiveEvent(led, LED_EVT_BLINK_1);   //260617_RL_add
    }         
    if (evt & EVT_LEDx_BLINK_ALT) { 
        led->blink_mode = LED_BLINK_MODE_ALT; 
        // StateMachine_SendEvent(&led->sm, LED_EVT_BLINK_2); 
        LED_SendActiveEvent(led, LED_EVT_BLINK_2);  //260617_RL_add  
    }
    if (evt & EVT_LEDx_BLINK_INTERVAL_2) { 
        led->blink_mode = LED_BLINK_MODE_INTERVAL_2; 
        // StateMachine_SendEvent(&led->sm, LED_EVT_BLINK_3); 
        LED_SendActiveEvent(led, LED_EVT_BLINK_3);   //260617_RL_add
    }
    if (evt & EVT_LEDx_BLINK_INTERVAL_3) { 
        led->blink_mode = LED_BLINK_MODE_INTERVAL_3; 
        // StateMachine_SendEvent(&led->sm, LED_EVT_BLINK_4); 
        LED_SendActiveEvent(led, LED_EVT_BLINK_4);   //260617_RL_add
    }
    // 频率
    if (evt & EVT_LEDx_FREQ1) { led->freq = LED_FREQ_1; }
    if (evt & EVT_LEDx_FREQ2) { led->freq = LED_FREQ_2; }
    if (evt & EVT_LEDx_FREQ3) { led->freq = LED_FREQ_3; }
    if (evt & EVT_LEDx_FREQ4) { led->freq = LED_FREQ_4; }        //260617_RL_add
		
    // 事件处理完 → 清空
    evt_group->event_bits = 0;
}

void LED_Task(void) {
	
	  uint32_t now = SysTick_GetTick();
	
    // 逐个处理LED：独立事件组 + 独立运行
    if (LED_COUNT >= 0) {
        // LED_ProcessEvent(0, &g_led[0], &g_led1_event_group);
        LED_ProcessEvent(0, &g_led[0], g_led[0].evt_led);
    }
    if (LED_COUNT >= 1) {
        // LED_ProcessEvent(1, &g_led[1], &g_led2_event_group);
        LED_ProcessEvent(1, &g_led[1], g_led[1].evt_led);
    }

    // 运行状态机 + 闪烁逻辑
    for (uint8_t i = 0; i < LED_COUNT; i++) {
        LED_Object_t *led = &g_led[i];

        if (led->sm.cur_state == LED_ST_RED_ON ||
            led->sm.cur_state == LED_ST_GREEN_ON ||
            led->sm.cur_state == LED_ST_YELLOW_ON)
        {
            led_set_color(led, led->color);
        }

        if (led->sm.cur_state == LED_ST_BLINK_1 ||
			led->sm.cur_state == LED_ST_BLINK_2 ||
			led->sm.cur_state == LED_ST_BLINK_3 ||
			led->sm.cur_state == LED_ST_BLINK_4) {
            led_blink_process(led);
        }

        if (led->sm.cur_state == LED_ST_OFF ||
            led->sm.cur_state == LED_ST_DISABLE)             //260613_RL_add
        {
            led_set_color(led, LED_COLOR_OFF);
        }
    }
}

// 10ms 节拍闪烁处理
static void led_blink_process(LED_Object_t *led) {
    uint32_t now = SysTick_GetTick();

    // 10ms 执行一次
    if (now - led->task_jiffies < 10) {
        return;
    }
    led->task_jiffies = now;

    uint16_t t_on  = freq_table[led->freq][0];
    uint16_t t_off = freq_table[led->freq][1];
    led->blink_jiffies++;

    // 基础亮灭
    if (led->pin_state == 0) {
        if (led->blink_jiffies >= t_off) {
            led->pin_state = 1;
            led->blink_jiffies = 0;
            led_set_color(led, led->color);
//					  now = SysTick_GetTick();
        }
    } else {
        if (led->blink_jiffies >= t_on) {
            led->pin_state = 0;
            led->blink_jiffies = 0;
            led_set_color(led, LED_COLOR_OFF);
					
						// ==============================================
						// ? 修复：只有熄灭闪烁，才判断是否需要计数
						// ==============================================
						if (led->blink_mode == LED_BLINK_MODE_COUNT ||
							  led->blink_mode == LED_BLINK_MODE_TIME ||
						    led->blink_mode == LED_BLINK_MODE_ALT ||
						    led->blink_mode == LED_BLINK_MODE_INTERVAL_2 ||
								led->blink_mode == LED_BLINK_MODE_INTERVAL_3)
						{
								led->cur_cnt++;  // 仅这两种模式需要计数
						}
        }
    }

    // 模式1：无限循环
    if (led->blink_mode == LED_BLINK_MODE_LOOP) {
        // 无需处理
    }
    // 模式2：次数闪烁
    else if (led->blink_mode == LED_BLINK_MODE_COUNT) {
        if (led->cur_cnt >= led->target_cnt) {
            StateMachine_SendEvent(&led->sm, LED_EVT_OFF);
            led->cur_cnt = 0;
        }
    }
    // 模式3：定时闪烁
    else if (led->blink_mode == LED_BLINK_MODE_TIME) {
        led->cur_time += 10;
        if (led->cur_time >= led->target_time) {
            StateMachine_SendEvent(&led->sm, LED_EVT_OFF);
            led->cur_time = 0;
        }
    }
    // 模式4：双色交替闪烁  (闪烁1次，更换颜色，频率不变)
    else if (led->blink_mode == LED_BLINK_MODE_ALT) {
        if (led->cur_cnt >= 1) {		
				
            LED_Color_t tmp = led->color;
            led->color = led->last_color;
            led->last_color = tmp;					
					  led->cur_cnt = 0;
					}
    }
    // 模式5：闪2次 + 间隔900ms
    else if (led->blink_mode == LED_BLINK_MODE_INTERVAL_2) {
         if (led->cur_cnt >= 2) {		
					    led_set_color(led, LED_COLOR_OFF);
					    led->pin_state = 1;
					    led->blink_jiffies = t_on;
					 
					    if(led->cur_cnt < 10){
								led->interval_time = SysTick_GetTick();	
							}
					    led->cur_cnt = 10;

						  if(SysTick_GetTick() - led->interval_time > led->interval_jiffies ){
							  	led->cur_cnt = 0;
							  	led->blink_jiffies = t_off;
								  led->pin_state = 0;			
//									led->interval_time = SysTick_GetTick();						 			
						  }
					}


    }
    // 模式6：闪3次 + 间隔500ms
    else if (led->blink_mode == LED_BLINK_MODE_INTERVAL_3) {
        if (led->cur_cnt >= 3) {		
					    led_set_color(led, LED_COLOR_OFF);
					    led->pin_state = 1;
					    led->blink_jiffies = t_on;
					
					    if(led->cur_cnt < 10){
								led->interval_time = SysTick_GetTick();	
							}
					    led->cur_cnt = 10;

						  if(SysTick_GetTick() - led->interval_time > led->interval_jiffies ){
							  	led->cur_cnt = 0;
							  	led->blink_jiffies = t_off;
								  led->pin_state = 0;					
//                  led->interval_time = SysTick_GetTick();									
						  }
					}


    }
		
}

// 状态机入口函数
static void led_off_enter(void)  {}
static void led_on_enter(void)   {}
static void led_blink_enter(void) {
    for (uint8_t i = 0; i < LED_COUNT; i++) {
        LED_Object_t *led = &g_led[i];
        if (led->sm.cur_state == LED_ST_BLINK_1  ||
					  led->sm.cur_state == LED_ST_BLINK_2  ||
				    led->sm.cur_state == LED_ST_BLINK_3  ||
					  led->sm.cur_state == LED_ST_BLINK_4) {
            led->pin_state = 0;
            led->blink_jiffies = 0;
            led->cur_time = 0;
            led->phase_jiffies = 0;
					
					if(led->blink_mode == LED_BLINK_MODE_ALT){
						led->color = led->alt_color_1;
						led->last_color = led->alt_color_2;
						led->freq = LED_FREQ_4;                     //260617——RL_fix: LED_FREQ_1 to LED_FREQ_4
						led_set_color(led, led->color);
						led_set_color(led, LED_COLOR_OFF);
					}
					if(led->blink_mode == LED_BLINK_MODE_INTERVAL_2){
						led->color = LED_COLOR_RED;
						led->interval_jiffies = 880;
						led->freq = LED_FREQ_2;
						led_set_color(led, led->color);
						led_set_color(led, LED_COLOR_OFF);
					}
					if(led->blink_mode == LED_BLINK_MODE_INTERVAL_3){
						led->color = LED_COLOR_RED;
						led->interval_jiffies = 480;
						led->freq = LED_FREQ_2;
						led_set_color(led, led->color);
						led_set_color(led, LED_COLOR_OFF);
					}
        }
    }
}


/**
 * @brief key状态 - key信息回调处理函数
 *
 * @param topic  消息主题ID（用于区分不同类型的消息）
 * @param data   指向位置信息数据缓冲区的指针
 * @param len    数据缓冲区长度（单位：字节）
 * @param prio   消息优先级
 *
 * @note 该函数用于接收并处理 key发布的按键数据
 */
void Led_Key_Callback(uint32_t topic, const void* data, uint16_t len, uint8_t prio){
    // 安全校验：数据长度必须匹配结构体长度
    if (data == NULL || len != sizeof(Key_Report_t)) {
        return; // 数据无效直接退出
    }

    // 把 data 转换成结构体指针
    const Key_Report_t* p_data = (const Key_Report_t*)data;

    switch (p_data->key_code)
    {
    case 0x00:   // M1_KEY
        /* code */
        break;
    case 0x01:   // M2_KEY
        /* code */
        break;
    case 0x02:   // ACC_KEY
        /* code */
        break;
    case 0x03:   // JH_KEY
        /* code */
        break;
    case 0x04:   // FL_KEY
        /* code */
        break;
    case 0x05:   // GDW_KEY
        /* code */
        break;
    case 0x20:   // KEY0+KEY1_LONG
        /* code */
        break;
    default:
        break;
    }

 
}

/**
260617_RL_add:
 * @brief 发送活跃事件（颜色ON / 闪烁），自动经LED_EVT_OFF中间态过渡
 *
 * @note  若当前处于活跃状态（非OFF/非DISABLE），先切到OFF再切到目标状态，
 *        保证任意活跃状态之间可自由切换，无需枚举全部交叉跳转表项。
 */
static void LED_SendActiveEvent(LED_Object_t *led, LED_Event_t evt)
{
    LED_State_t cur = (LED_State_t)led->sm.cur_state;

    /* 当前处于活跃状态时，先过渡到 OFF */
    if (cur != LED_ST_OFF && cur != LED_ST_DISABLE) {
        StateMachine_SendEvent(&led->sm, LED_EVT_OFF);
    }

    /* 从 OFF（或原状态）跳转到目标状态 */
    StateMachine_SendEvent(&led->sm, evt);
}

//设置led事件
void led_set_event(uint8_t led_idx, uint32_t evt_bit)
{
        g_led[led_idx].evt_led->event_bits = evt_bit;
}


