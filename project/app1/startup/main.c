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

#if SYS_ENABLE_UDS
#include "../app/can/uds/uds_ota.h"
#endif

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




extern void bsp_storage_test(void);
int main(void) {

		MAIN_D("===== main(): app1 =====\r\n");
    LL_PERIPH_WE(LL_PERIPH_SEL);
    // Ӳ����ʼ��...

    BSP_CLK_Init();

    // 1. ��ʼ�� SysTick������ 1ms �жϣ�1000Hz��
    SysTick_Init(1000U);

    // 2. ��ȫ���жϣ����룡��
    __enable_irq();
	

    System_Init();  // 系统初始化（含 CAN 硬件初始化）

#if SYS_ENABLE_UDS
    UdsOta_App_CheckPendingAck();  // Phase 3: 检查并补发 UDS 挂起响应
#endif

    //    Test_RunAll();  // ִ�����в���
    LL_PERIPH_WP(LL_PERIPH_SEL);

    while (1) {
#if SYS_ENABLE_UDS
        if (g_swdt_feed_disable == 0U) {
            SWDT_FeedDog();
        }
#else
			SWDT_FeedDog();
#endif
	    uint32_t now = SysTick_GetTick();
        Sys_Schedule_Run();  // 调度器运行
#if SYS_ENABLE_UDS
        UdsOta_Poll();
#endif
//        bsp_storage_test();
//			  LOG_INFO("Sys_Schedule_Run");
//			  DDL_DelayMS(200);
    }
}

// 1ms ��ʱ���жϷ�����
void Timer1ms_IRQHandler(void) {
//    SysTick_Inc();
}



/**
 * @brief SysTick �жϷ�����
 */
void SysTick_Handler(void)
{
    SysTick_IncTick();  // �ٷ����ṩ��ÿ���ж� +1ms
}
