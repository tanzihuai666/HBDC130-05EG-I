#include "gd32f10x.h"
#include "power_ctrl.h"
#include "sensor_manager.h"
#include "fault_manager.h"
#include "ipmi_dispatch.h"

static volatile uint32_t g_ms_tick;

void SysTick_Handler(void)
{
    g_ms_tick++;
}

static void board_init(void)
{
    SystemInit();
    SysTick_Config(SystemCoreClock / 1000u);
    power_ctrl_init();
    ipmi_init(power_ctrl_get_ipmb_addr_8bit(), power_ctrl_get_ga_id());
    fault_manager_init();
}

int main(void)
{
    uint32_t last_10ms = 0u;
    uint32_t last_100ms = 0u;

    board_init();

    while (1) {
        power_ctrl_task_1ms();

        if ((uint32_t)(g_ms_tick - last_10ms) >= 10u) {
            last_10ms = g_ms_tick;
            fault_manager_task_10ms();
        }

        if ((uint32_t)(g_ms_tick - last_100ms) >= 100u) {
            last_100ms = g_ms_tick;
            sensor_manager_task_100ms();
        }

        /* 后续接入 ipmb_driver_task()：I2C1 Slave RX + Master TX Response 队列。 */
    }
}
