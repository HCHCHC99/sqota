#include "sys_init.h"
#include "sys_sched.h"
#include "sys_tick.h"
//#include "test/test_main.h"


#include "hc32_ll.h"
#include "hc32_ll_aos.h"
#include "hc32_ll_clk.h"
#include "hc32_ll_dma.h"
#include "hc32_ll_efm.h"
#include "hc32_ll_fcg.h"
#include "hc32_ll_fcm.h"
#include "hc32_ll_gpio.h"
#include "hc32_ll_i2c.h"
#include "hc32_ll_i2s.h"
#include "hc32_ll_interrupts.h"
#include "hc32_ll_keyscan.h"
#include "hc32_ll_pwc.h"
#include "hc32_ll_spi.h"
#include "hc32_ll_sram.h"
#include "hc32_ll_usart.h"
#include "hc32_ll_utility.h"

#include "log_rtt.h"

/**
 * @brief  BSP clock initialize.
 *         Set board system clock to MPLL@200MHz
 * @param  None
 * @retval None
 */
__WEAKDEF void BSP_CLK_Init(void)
{
    stc_clock_xtal_init_t     stcXtalInit;
    stc_clock_pll_init_t      stcMpllInit;

    GPIO_AnalogCmd(GPIO_PORT_H, GPIO_PIN_00 | GPIO_PIN_01, ENABLE);
    (void)CLK_XtalStructInit(&stcXtalInit);
    (void)CLK_PLLStructInit(&stcMpllInit);

    /* Set bus clk div. */
    CLK_SetClockDiv(CLK_BUS_CLK_ALL, (CLK_HCLK_DIV1 | CLK_EXCLK_DIV2 | CLK_PCLK0_DIV1 | CLK_PCLK1_DIV2 | \
                                      CLK_PCLK2_DIV4 | CLK_PCLK3_DIV4 | CLK_PCLK4_DIV2));

    /* Config Xtal and enable Xtal */
    stcXtalInit.u8Mode = CLK_XTAL_MD_OSC;
    stcXtalInit.u8Drv = CLK_XTAL_DRV_ULOW;
    stcXtalInit.u8State = CLK_XTAL_ON;
    stcXtalInit.u8StableTime = CLK_XTAL_STB_2MS;
    (void)CLK_XtalInit(&stcXtalInit);

    /* MPLL config (XTAL / pllmDiv * plln / PllpDiv = 200M). */
    stcMpllInit.PLLCFGR = 0UL;
    stcMpllInit.PLLCFGR_f.PLLM = 1UL - 1UL;
    stcMpllInit.PLLCFGR_f.PLLN = 50UL - 1UL;
    stcMpllInit.PLLCFGR_f.PLLP = 2UL - 1UL;
    stcMpllInit.PLLCFGR_f.PLLQ = 2UL - 1UL;
    stcMpllInit.PLLCFGR_f.PLLR = 2UL - 1UL;
    stcMpllInit.u8PLLState = CLK_PLL_ON;
    stcMpllInit.PLLCFGR_f.PLLSRC = CLK_PLL_SRC_XTAL;
    (void)CLK_PLLInit(&stcMpllInit);
    /* Wait MPLL ready. */
    while (SET != CLK_GetStableStatus(CLK_STB_FLAG_PLL)) {
        ;
    }

    /* sram init include read/write wait cycle setting */
    SRAM_SetWaitCycle(SRAM_SRAMH, SRAM_WAIT_CYCLE0, SRAM_WAIT_CYCLE0);
    SRAM_SetWaitCycle((SRAM_SRAM12 | SRAM_SRAM3 | SRAM_SRAMR), SRAM_WAIT_CYCLE1, SRAM_WAIT_CYCLE1);

    /* flash read wait cycle setting */
    (void)EFM_SetWaitCycle(EFM_WAIT_CYCLE5);
    /* 3 cycles for 126MHz ~ 200MHz */
    GPIO_SetReadWaitCycle(GPIO_RD_WAIT3);
    /* Switch driver ability */
    (void)PWC_HighSpeedToHighPerformance();
    /* Switch system clock source to MPLL. */
    CLK_SetSysClockSrc(CLK_SYSCLK_SRC_PLL);
    /* Reset cache ram */
    EFM_CacheRamReset(ENABLE);
    EFM_CacheRamReset(DISABLE);
    /* Enable cache */
    EFM_CacheCmd(ENABLE);
}

#define LL_PERIPH_SEL       (LL_PERIPH_GPIO | LL_PERIPH_FCG | LL_PERIPH_PWC_CLK_RMU | LL_PERIPH_EFM | LL_PERIPH_SRAM)


#define APP1_START_ADDR  0x0001A000UL

extern void bsp_storage_test(void);
int main(void) {
    SCB->VTOR = APP1_START_ADDR;
	
    LL_PERIPH_WE(LL_PERIPH_SEL);
    // 硬件初始化...

    BSP_CLK_Init();

    // 1. 初始化 SysTick，配置 1ms 中断（1000Hz）
    SysTick_Init(1000U);

    // 2. 打开全局中断（必须！）
    __enable_irq();
	

    System_Init();  // 系统初始化

    //    Test_RunAll();  // 执行所有测试
    LL_PERIPH_WP(LL_PERIPH_SEL);

    while (1) {
	    uint32_t now = SysTick_GetTick();
        Sys_Schedule_Run();  // 调度器运行
//        bsp_storage_test();
//			  LOG_INFO("Sys_Schedule_Run");
//			  DDL_DelayMS(200);
    }
}

// 1ms 定时器中断服务函数
void Timer1ms_IRQHandler(void) {
//    SysTick_Inc();
}



/**
 * @brief SysTick 中断服务函数
 */
void SysTick_Handler(void)
{
    SysTick_IncTick();  // 官方库提供，每次中断 +1ms
}
