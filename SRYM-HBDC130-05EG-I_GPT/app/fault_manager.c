/**
 * @file    fault_manager.c
 * @brief   故障检测、FAIL 输出控制和严重故障关断联动实现。
 *          本模块每 10ms 汇总传感器层生成的电压/温度离散状态，并读取 PB8
 *          PWOUT_TEST_MCU 硬件检测信号，形成统一 32 位故障字。任意故障都会拉低 FAIL，
 *          输入电压异常、硬件 PWOUT 异常或严重过温还会触发电源强制关断。
 * @author  GPT
 * @date    2026-06-15
 * @version V0.4
 */
#include "fault_manager.h"
#include "sensor_manager.h"
#include "power_ctrl.h"
#include "bsp_gpio.h"

/* 故障字 bit0：输入电压低于或高于允许范围。 */
#define FAULT_INPUT_VOLTAGE     (1UL << 0)

/* 故障字 bit1：任一输出电压低于或高于允许范围。 */
#define FAULT_OUTPUT_VOLTAGE    (1UL << 1)

/* 故障字 bit2：温度超过告警阈值，但尚未达到强制关断阈值。 */
#define FAULT_OVERTEMP_WARN     (1UL << 2)

/* 故障字 bit3：温度超过严重阈值，需要立即关断受控电源。 */
#define FAULT_OVERTEMP_SHUTDOWN (1UL << 3)

/* 故障字 bit4：PB8 PWOUT_TEST_MCU 硬件输入指示异常。 */
#define FAULT_PWOUT_ABNORMAL    (1UL << 4)

/* 当前 10ms 周期计算得到的活动故障集合。 */
static uint32_t g_fault_bits;

/* 上一次已打印的故障集合，用于只在故障变化时输出日志。 */
static uint32_t g_last_fault_bits;

/**
 * @brief  初始化故障管理状态并释放低有效 FAIL 输出。
 * @param  无。
 * @retval 无。
 */
void fault_manager_init(void)
{
    g_fault_bits = 0u;
    g_last_fault_bits = 0u;

    /* false 表示没有故障，PC6 输出高电平释放 FAIL。 */
    bsp_gpio_set_fail(false);

    APP_LOGI("Fault manager initialized");
}

/**
 * @brief  10ms 周期故障检测与保护任务。
 * @param  无。
 * @retval 无。
 */
void fault_manager_task_10ms(void)
{
    uint16_t voltage_status;
    uint16_t temperature_status;

    /* 从传感器层读取已经根据工程量计算完成的 FRU 离散状态。 */
    voltage_status = sensor_get_fru_voltage_bits();
    temperature_status = sensor_get_fru_temperature_bits();

    /* 每个周期重新构造故障字，避免已经恢复的故障永久残留。 */
    g_fault_bits = 0u;

    /* FRU Voltage bit2 表示输入电压异常。 */
    if ((voltage_status & 0x0004u) != 0u) {
        g_fault_bits |= FAULT_INPUT_VOLTAGE;
    }

    /* FRU Voltage bit3 表示输出电压异常。 */
    if ((voltage_status & 0x0008u) != 0u) {
        g_fault_bits |= FAULT_OUTPUT_VOLTAGE;
    }

    /* FRU Temperature bit1 表示过温告警。 */
    if ((temperature_status & 0x0002u) != 0u) {
        g_fault_bits |= FAULT_OVERTEMP_WARN;
    }

    /* FRU Temperature bit2 表示严重过温。 */
    if ((temperature_status & 0x0004u) != 0u) {
        g_fault_bits |= FAULT_OVERTEMP_SHUTDOWN;
    }

    /* PB8 为高表示硬件检测到输入电源异常。 */
    if (!power_ctrl_input_voltage_ok()) {
        g_fault_bits |= FAULT_PWOUT_ABNORMAL;
    }

    /* 仅在故障字变化时输出，防止 10ms 周期连续打印相同日志。 */
    if (g_fault_bits != g_last_fault_bits) {
        APP_LOGW("Fault bits changed: 0x%08lX -> 0x%08lX",
                 (unsigned long)g_last_fault_bits,
                 (unsigned long)g_fault_bits);
        g_last_fault_bits = g_fault_bits;
    }

    if (g_fault_bits != 0u) {
        /* 任意活动故障都拉低 FAIL 输出。 */
        bsp_gpio_set_fail(true);

        /*
         * 输入电压异常、严重过温和 PWOUT 硬件异常属于需要立即关断的严重故障；
         * 普通输出电压异常和温度告警目前只上报 FAIL，不直接执行强制关断。
         */
        if ((g_fault_bits &
             (FAULT_INPUT_VOLTAGE |
              FAULT_OVERTEMP_SHUTDOWN |
              FAULT_PWOUT_ABNORMAL)) != 0u) {
            power_ctrl_force_off();
        }
    } else if (power_ctrl_enable_active()) {
        /* 无活动故障且模块处于使能状态时释放 FAIL。 */
        bsp_gpio_set_fail(false);
    }
}

/**
 * @brief  查询当前是否存在任意活动故障。
 * @param  无。
 * @retval true=至少一个故障位有效；false=无故障。
 */
bool fault_manager_has_active_fault(void)
{
    return (g_fault_bits != 0u) ? true : false;
}

/**
 * @brief  获取当前完整 32 位故障字。
 * @param  无。
 * @retval 当前故障位集合。
 */
uint32_t fault_manager_get_bits(void)
{
    return g_fault_bits;
}
