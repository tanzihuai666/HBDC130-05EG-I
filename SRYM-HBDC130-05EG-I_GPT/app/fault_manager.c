/**
 * @file    fault_manager.c
 * @brief   故障检测与保护联动。
 *          以传感器离散状态、PB8 PWOUT_TEST_MCU 和温度/电压状态为依据，控制 FAIL*，
 *          并在输入异常或严重过温时强制关断电源输出。
 * @author  GPT
 * @date    2026-06-12
 * @version V0.3
 */
#include "fault_manager.h"
#include "sensor_manager.h"
#include "power_ctrl.h"
#include "bsp_gpio.h"

/* 故障位定义：供 Self Test、日志和后续故障日志存储使用。 */
#define FAULT_INPUT_VOLTAGE     (1UL << 0)  /* 输入电压越限。 */
#define FAULT_OUTPUT_VOLTAGE    (1UL << 1)  /* 输出电压越限。 */
#define FAULT_OVERTEMP_WARN     (1UL << 2)  /* 温度告警。 */
#define FAULT_OVERTEMP_SHUTDOWN (1UL << 3)  /* 温度严重越限，需要关断。 */
#define FAULT_PWOUT_ABNORMAL    (1UL << 4)  /* PB8 硬件输入电压异常。 */

static uint32_t g_fault_bits;       /* 当前故障位集合。 */
static uint32_t g_last_fault_bits;  /* 日志去重用上一拍故障位。 */

/**
 * @brief  初始化故障管理模块。
 * @param  无。
 * @retval 无。
 */
void fault_manager_init(void)
{
    g_fault_bits = 0u;
    g_last_fault_bits = 0u;
    bsp_gpio_set_fail(false);
    APP_LOGI("fault: init ok");
}

/**
 * @brief  10ms 故障检测任务。
 * @param  无。
 * @retval 无。
 */
void fault_manager_task_10ms(void)
{
    uint16_t fru_v = sensor_get_fru_voltage_bits();
    uint16_t fru_t = sensor_get_fru_temperature_bits();

    g_fault_bits = 0u;
    if (fru_v & 0x0004u) g_fault_bits |= FAULT_INPUT_VOLTAGE;
    if (fru_v & 0x0008u) g_fault_bits |= FAULT_OUTPUT_VOLTAGE;
    if (fru_t & 0x0002u) g_fault_bits |= FAULT_OVERTEMP_WARN;
    if (fru_t & 0x0004u) g_fault_bits |= FAULT_OVERTEMP_SHUTDOWN;
    if (!power_ctrl_input_voltage_ok()) g_fault_bits |= FAULT_PWOUT_ABNORMAL;

    if (g_fault_bits != g_last_fault_bits) {
        APP_LOGW("fault: bits 0x%08lX -> 0x%08lX", (unsigned long)g_last_fault_bits, (unsigned long)g_fault_bits);
        g_last_fault_bits = g_fault_bits;
    }

    if (g_fault_bits != 0u) {
        bsp_gpio_set_fail(true);
        if ((g_fault_bits & (FAULT_INPUT_VOLTAGE | FAULT_OVERTEMP_SHUTDOWN | FAULT_PWOUT_ABNORMAL)) != 0u) {
            power_ctrl_force_off();
        }
    } else if (power_ctrl_enable_active()) {
        bsp_gpio_set_fail(false);
    }
}

/**
 * @brief  查询是否存在活动故障。
 * @param  无。
 * @retval true=存在故障，false=无故障。
 */
bool fault_manager_has_active_fault(void)
{
    return (g_fault_bits != 0u) ? true : false;
}

/**
 * @brief  获取当前故障位。
 * @param  无。
 * @retval 故障位集合。
 */
uint32_t fault_manager_get_bits(void)
{
    return g_fault_bits;
}
