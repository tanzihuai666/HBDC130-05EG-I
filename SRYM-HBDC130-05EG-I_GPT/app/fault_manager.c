#include "fault_manager.h"
#include "sensor_manager.h"
#include "power_ctrl.h"
#include "bsp_gpio.h"

#define FAULT_INPUT_VOLTAGE     (1UL << 0)
#define FAULT_OUTPUT_VOLTAGE    (1UL << 1)
#define FAULT_OVERTEMP_WARN     (1UL << 2)
#define FAULT_OVERTEMP_SHUTDOWN (1UL << 3)
#define FAULT_PWOUT_ABNORMAL    (1UL << 4)

static uint32_t g_fault_bits;

void fault_manager_init(void)
{
    g_fault_bits = 0u;
    bsp_gpio_set_fail(false);
}

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

    if (g_fault_bits != 0u) {
        bsp_gpio_set_fail(true);
        if ((g_fault_bits & (FAULT_INPUT_VOLTAGE | FAULT_OVERTEMP_SHUTDOWN | FAULT_PWOUT_ABNORMAL)) != 0u) {
            power_ctrl_force_off();
        }
    } else if (power_ctrl_enable_active()) {
        bsp_gpio_set_fail(false);
    }
}

bool fault_manager_has_active_fault(void)
{
    return (g_fault_bits != 0u) ? true : false;
}

uint32_t fault_manager_get_bits(void)
{
    return g_fault_bits;
}
