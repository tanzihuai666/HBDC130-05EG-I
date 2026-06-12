#include "fault_manager.h"
#include "sensor_manager.h"
#include "power_ctrl.h"

#define FAULT_INPUT_VOLTAGE     (1UL << 0)
#define FAULT_OUTPUT_VOLTAGE    (1UL << 1)
#define FAULT_OVERTEMP_WARN     (1UL << 2)
#define FAULT_OVERTEMP_SHUTDOWN (1UL << 3)

static uint32_t g_fault_bits;

void fault_manager_init(void)
{
    g_fault_bits = 0u;
    GPIO_SetBits(GPIOC, GPIO_PIN_6);
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

    if (g_fault_bits != 0u) {
        GPIO_ResetBits(GPIOC, GPIO_PIN_6);
        if ((g_fault_bits & (FAULT_INPUT_VOLTAGE | FAULT_OVERTEMP_SHUTDOWN)) != 0u) {
            power_ctrl_force_off();
        }
    } else if (power_ctrl_enable_active()) {
        GPIO_SetBits(GPIOC, GPIO_PIN_6);
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
