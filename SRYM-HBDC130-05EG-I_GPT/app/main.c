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

static void board_init(void)
{
    uint8_t own_addr_8bit;

    SystemInit();
    bsp_timer_init_1ms();
    bsp_gpio_init();
    bsp_uart1_init(115200u);
    bsp_adc_dma_init();
    power_ctrl_init();

    own_addr_8bit = power_ctrl_get_ipmb_addr_8bit();
    ipmi_init(own_addr_8bit, power_ctrl_get_ga_id());
    ipmb_driver_init(own_addr_8bit);
    bsp_i2c1_ipmb_init((uint8_t)(own_addr_8bit >> 1));

    fault_manager_init();
    bsp_watchdog_init();
}

int main(void)
{
    uint32_t last_10ms = 0u;
    uint32_t last_100ms = 0u;

    board_init();

    while (1) {
        uint32_t now = bsp_millis();
        power_ctrl_task_1ms();
        ipmb_driver_task();

        if ((uint32_t)(now - last_10ms) >= 10u) {
            bsp_adc_values_t adc;
            last_10ms = now;
            bsp_adc_dma_task_10ms();
            bsp_adc_get_values(&adc);
            sensor_set_adc_values(adc.vin_v, adc.v12_v, adc.v5_v, adc.v33_v, adc.vm12_v, adc.v28_v,
                                  adc.vin_i, adc.i12_a, adc.i33_a, adc.i5_a, adc.temp_c);
            fault_manager_task_10ms();
        }

        if ((uint32_t)(now - last_100ms) >= 100u) {
            last_100ms = now;
            sensor_manager_task_100ms();
        }

        bsp_watchdog_feed();
    }
}
