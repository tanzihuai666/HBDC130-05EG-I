/**
 * @file    main.c
 * @brief   系统入口与主循环调度。
 *          负责初始化 BSP、应用层、IPMI/IPMB 协议栈，并按 1ms/10ms/100ms
 *          周期调度电源控制、ADC 采样、故障检测、传感器刷新和 IPMB 响应发送。
 * @author  GPT
 * @date    2026-06-12
 * @version V0.3
 */
#include "gd32f10x.h"
#include "power_ctrl.h"
#include "sensor_manager.h"
#include "fault_manager.h"
#include "ipmi_dispatch.h"
#include "ipmb_driver.h"
#include "bsp_gpio.h"
#include "bsp_timer.h"
#include "bsp_adc_dma.h"
#include "bsp_uart.h"
#include "bsp_watchdog.h"
#include "bsp_i2c.h"

/**
 * @brief  板级和应用层初始化。
 * @param  无。
 * @retval 无。
 * @note   初始化顺序很关键：UART 先于日志输出；GPIO 先于电源控制；
 *         IPMI 数据库先于 IPMB 接收；I2C 最后打开，避免未准备好时收到请求。
 */
static void board_init(void)
{
    uint8_t own_addr_8bit;

    SystemInit();
    bsp_timer_init_1ms();
    bsp_gpio_init();
    bsp_uart1_init(115200u);
    APP_LOGI("boot: HBDC130-05EG-I_GPT fw=%02x.%02x", APP_FW_MAJOR_BCD, APP_FW_MINOR_BCD);

    bsp_adc_dma_init();
    APP_LOGI("adc: dma scan started");

    power_ctrl_init();
    own_addr_8bit = power_ctrl_get_ipmb_addr_8bit();
    APP_LOGI("power: GA=%u IPMB=0x%02X", power_ctrl_get_ga_id(), own_addr_8bit);

    ipmi_init(own_addr_8bit, power_ctrl_get_ga_id());
    ipmb_driver_init(own_addr_8bit);
    bsp_i2c1_ipmb_init((uint8_t)(own_addr_8bit >> 1));
    APP_LOGI("ipmb: i2c1 slave ready addr7=0x%02X", (unsigned int)(own_addr_8bit >> 1));

    fault_manager_init();
    bsp_watchdog_init();
    APP_LOGI("system: init done");
}

/**
 * @brief  主函数。
 * @param  无。
 * @retval int 裸机程序不返回。
 */
int main(void)
{
    uint32_t last_10ms = 0u;
    uint32_t last_100ms = 0u;

    board_init();

    while (1) {
        uint32_t now = bsp_millis();

        /* 1ms 任务：电源输入信号去抖和状态机更新，必须高频执行。 */
        power_ctrl_task_1ms();

        /* IPMB 任务：把 I2C 中断收到的请求解析为 IPMI，再排队发送响应。 */
        ipmb_driver_task();

        if ((uint32_t)(now - last_10ms) >= 10u) {
            bsp_adc_values_t adc;
            last_10ms = now;

            /* 10ms 任务：更新 ADC 工程量，并基于最新值做故障保护。 */
            bsp_adc_dma_task_10ms();
            bsp_adc_get_values(&adc);
            sensor_set_adc_values(adc.vin_v, adc.v12_v, adc.v5_v, adc.v33_v, adc.vm12_v, adc.v28_v,
                                  adc.vin_i, adc.i12_a, adc.i33_a, adc.i5_a, adc.temp_c);
            fault_manager_task_10ms();
        }

        if ((uint32_t)(now - last_100ms) >= 100u) {
            last_100ms = now;

            /* 100ms 任务：刷新 IPMI 传感器 raw/event 状态，供 Get Sensor Reading 读取。 */
            sensor_manager_task_100ms();
        }

        /* 当前策略：所有关键任务能正常回到主循环时才喂狗。 */
        bsp_watchdog_feed();
    }
}
