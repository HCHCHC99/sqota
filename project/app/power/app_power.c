#include "sys_config.h"
#include "app_power.h"
#include "app_pos.h"
#include "msg_pubsub.h"
#include "msg_topics.h"
#include "power_module.h"

#if SYS_ENABLE_POWER

extern const power_hw_cfg_t POWER_HW_CFG_VM;
extern const power_hw_cfg_t POWER_HW_CFG_M1;
extern const power_hw_cfg_t POWER_HW_CFG_M2;

// 实例句柄
power_handle_t power_vm;
power_handle_t power_m1;
power_handle_t power_m2;

// 软件配置实例
static const power_sw_cfg_t POWER_SW_CFG_VM = {
    .over_volt_th = 32.0f,
    .under_volt_th = 8.0f,
    .over_cur_th = 0.0f,
    
    .volt_hyst = 2.0f,
    .volt_delay_ms = 3000,
    
    
    .cbVoltageStatus = NULL,
    .cbCurrentStatus = NULL,
    .cbPolarityDetect = NULL,
};

static const power_sw_cfg_t POWER_SW_CFG_M1 = {
    .over_volt_th = 0.0f,
    .under_volt_th = 0.0f,
    .over_cur_th = 10.0f,
    
    .volt_hyst = 0.0f,
    .volt_delay_ms = 0,
    
    .cbVoltageStatus = NULL,
    .cbCurrentStatus = NULL,
    .cbPolarityDetect = NULL,
};

static const power_sw_cfg_t POWER_SW_CFG_M2 = {
    .over_volt_th = 0.0f,
    .under_volt_th = 0.0f,
    .over_cur_th = 10.0f,
    
    .volt_hyst = 0.0f,
    .volt_delay_ms = 0,
    
    .cbVoltageStatus = NULL,
    .cbCurrentStatus = NULL,
    .cbPolarityDetect = NULL,
};

// 完整配置（组合硬件和软件）
static const power_cfg_t POWER_CFG_VM = {
    .hw = &POWER_HW_CFG_VM,
    .sw = &POWER_SW_CFG_VM,
};

static const power_cfg_t POWER_CFG_M1 = {
    .hw = &POWER_HW_CFG_M1,
    .sw = &POWER_SW_CFG_M1,
};

static const power_cfg_t POWER_CFG_M2 = {
    .hw = &POWER_HW_CFG_M2,
    .sw = &POWER_SW_CFG_M2,
};

// 状态上报结构体实例
power_upd_t s_power_msg_vm = {
    .name = "VM",
    .idx = 0,
    .polarity = 0,
    .voltage = 0,
    .current = 0,
    .status_volt = POWER_STATUS_NORMAL,
    .status_curr = POWER_STATUS_NORMAL
};

power_upd_t s_power_msg_m1 = {
    .name = "M1",
    .idx = 1,
    .polarity = 0,
    .voltage = 0,
    .current = 0,
    .status_volt = POWER_STATUS_NORMAL,
    .status_curr = POWER_STATUS_NORMAL
};

power_upd_t s_power_msg_m2 = {
    .name = "M2",
    .idx = 2,
    .polarity = 0,
    .voltage = 0,
    .current = 0,
    .status_volt = POWER_STATUS_NORMAL,
    .status_curr = POWER_STATUS_NORMAL
};


/**
 * @brief 处理VM数据（从缓冲区读取并滤波 -- 先以最简单的方法实现）
 */
static void process_vm_data(void)
{
    float voltage_buffer[10];
    uint16_t read_count;
    float sum = 0;
    uint8_t i;
    
    // 从缓冲区读取10个电压数据
    power_buffer_read_voltage(&power_vm, voltage_buffer, 20, &read_count);
    
    if (read_count > 0) {
        // 对读取到的数据进行平均
        for (i = 0; i < read_count; i++) {
            sum += voltage_buffer[i];
        }
        power_vm.power_upd.voltage = sum / read_count;
    }
}


void app_adc_get_vm(void)
{         
    ADC_Start(CM_ADC1);
}

static void check_voltage_status(power_handle_t *ph)
{
    static uint32_t restore_start = 0;
    static uint8_t waiting = 0;
    static uint8_t fault = 0;

    const power_cfg_t *cfg = ph->cfg;
    const power_sw_cfg_t *sw = cfg->sw;
    const power_hw_cfg_t *hw = cfg->hw;
    uint32_t now = SysTick_GetTick();

    if (!hw->en_voltage) {
        ph->power_upd.status_volt = POWER_STATUS_NORMAL;
        fault = 0;
        waiting = 0;
        return;
    }

    float volt = ph->power_upd.voltage;
    uint8_t is_over = (volt > sw->over_volt_th);
    uint8_t is_under = (volt < sw->under_volt_th);
    
    float volt_hyst = (sw->volt_hyst > 0) ? sw->volt_hyst : 2.0f;
    uint32_t delay_ms = (sw->volt_delay_ms > 0) ? sw->volt_delay_ms : 3000;

    if (fault != 0) {
        uint8_t recover = (fault == 2) ?
                          (volt < (sw->over_volt_th - volt_hyst)) :
                          (volt > (sw->under_volt_th + volt_hyst));
        if (recover) {
            if (!waiting) {
                waiting = 1;
                restore_start = now;
            } else if ((now - restore_start) >= delay_ms) {
                fault = 0;
                waiting = 0;
            }
        } else {
            waiting = 0;
        }
    } else {
        if (is_over) {
            fault = 2;
            waiting = 0;
        } else if (is_under) {
            fault = 1;
            waiting = 0;
        }
    }

    if (fault == 2)      ph->power_upd.status_volt = POWER_STATUS_OVER_VOLT;
    else if (fault == 1) ph->power_upd.status_volt = POWER_STATUS_UNDER_VOLT;
    else                 ph->power_upd.status_volt = POWER_STATUS_NORMAL;
}

static void check_current_status(power_handle_t *ph)
{
    const power_cfg_t *cfg = ph->cfg;
    const power_sw_cfg_t *sw = cfg->sw;
    const power_hw_cfg_t *hw = cfg->hw;
    
    if (!hw->en_current || sw->over_cur_th == 0) {
        ph->power_upd.status_curr = POWER_STATUS_NORMAL;
        ph->cur_over_count = 0;
        return;
    }
    
    float curr = (float)ph->power_upd.current;
    uint8_t fault_threshold = (sw->cur_fault_count > 0) ? sw->cur_fault_count : 3;
    
    if (curr > sw->over_cur_th) {
        // 过流：计数加1
        ph->cur_over_count++;
        if (ph->cur_over_count >= fault_threshold) {
            ph->power_upd.status_curr = POWER_STATUS_OVER_CUR;
        }
    } else {
        // 正常：立即清零计数和状态
        ph->cur_over_count = 0;
        ph->power_upd.status_curr = POWER_STATUS_NORMAL;
    }
}

void app_power_stat_upd(void)
{
    s_power_msg_vm.voltage = power_vm.power_upd.voltage;
    s_power_msg_vm.current = power_vm.power_upd.current;
    s_power_msg_vm.status_volt = power_vm.power_upd.status_volt;
    s_power_msg_vm.status_curr = power_vm.power_upd.status_curr;
    
    s_power_msg_m1.voltage = power_m1.power_upd.voltage;
    s_power_msg_m1.current = power_m1.power_upd.current;
    s_power_msg_m1.status_volt = power_m1.power_upd.status_volt;
    s_power_msg_m1.status_curr = power_m1.power_upd.status_curr;
    
    s_power_msg_m2.voltage = power_m2.power_upd.voltage;
    s_power_msg_m2.current = power_m2.power_upd.current;
    s_power_msg_m2.status_volt = power_m2.power_upd.status_volt;
    s_power_msg_m2.status_curr = power_m2.power_upd.status_curr;
}

void app_power_check_stat(void)
{
    check_voltage_status(&power_vm);
    check_current_status(&power_m1);
    check_current_status(&power_m2);
}

static void power_publish_state(void)
{
    Msg_Publish(TOPIC_PWR_STATE_UPDATED, &s_power_msg_vm, sizeof(power_upd_t), MSG_PRIO_HIGH);
    Msg_Publish(TOPIC_PWR_STATE_UPDATED, &s_power_msg_m1, sizeof(power_upd_t), MSG_PRIO_HIGH);
    Msg_Publish(TOPIC_PWR_STATE_UPDATED, &s_power_msg_m2, sizeof(power_upd_t), MSG_PRIO_HIGH);
}

void app_power_init(void)
{
    power_module_init(&power_vm, &POWER_CFG_VM);
    power_module_init(&power_m1, &POWER_CFG_M1);
    power_module_init(&power_m2, &POWER_CFG_M2);

    // power_module_start(&power_m1);
}

void app_power_task(void)
{
    static uint32_t last_vm_process_ms = 0;
    uint32_t now = SysTick_GetTick();
    
    app_adc_get_vm();
    
    // VM数据：每20ms处理一次（从缓冲区读取20个样本取平均），这里有一点点疑问，一个模块的任务有不同处理时间做怎么整比较好？
    if (now - last_vm_process_ms >= 20) {
        last_vm_process_ms = now;
        process_vm_data();
    }
    
    app_power_check_stat();
    app_power_stat_upd();
    power_publish_state();
}

//序列A采集完成中断回调函数
void ADC1_SeqA_IrqCallback(void)
{
    ADC_ClearStatus(CM_ADC1, ADC_FLAG_EOCA);
    power_module_process(&power_vm); //这里是采集数据，换算成电压值写入环形数组
}

//序列B采集完成中断回调函数
void ADC1_SeqB_IrqCallback(void)
{
    ADC_ClearStatus(CM_ADC1, ADC_FLAG_EOCB);
    power_module_process(&power_m1);
    power_module_process(&power_m2); 
    app_pos_data_process();  
}

#endif

