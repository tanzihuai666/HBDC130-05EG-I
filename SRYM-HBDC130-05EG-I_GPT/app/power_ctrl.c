#include "power_ctrl.h"
#include "bsp_gpio.h"

#define CTRL_DEBOUNCE_MS       5u

/*
 * GPIO mapping from software requirement:
 * EN input      PB13, MCU side low active after inverter
 * INH input     PB14, MCU side low active after inverter
 * NVMRO input   PB9,  MCU side low active after inverter
 * PWOUT_TEST    PB8,  low = input voltage normal, high = abnormal
 * EN1 output    PC12, high enables +3.3V_AUX/+5V
 * INH2 output   PB4,  low enables +12V
 * INH1 output   PB5,  low enables +28V/-12V
 * FAIL output   PC6,  low active, handled by fault_manager via BSP
 * GA0/GA1/GA2   PC7/PC8/PA8
 */

static power_state_t g_power_state = POWER_STATE_OFF;
static uint8_t g_ga_id;
static uint8_t g_ipmb_addr_8bit = IPMB_PM_BASE_ADDR_8BIT;
static uint8_t g_en_cnt;
static uint8_t g_inh_cnt;
static uint8_t g_nvmro_cnt;
static bool g_en_active;
static bool g_inh_active;
static bool g_nvmro_allows_update;

static bool input_low_active(GPIO_TypeDef *port, uint16_t pin)
{
    return (bsp_gpio_read(port, pin) == BSP_GPIO_LOW) ? true : false;
}

static void debounce_bool(bool sample, uint8_t *cnt, bool *state)
{
    if (sample == *state) {
        *cnt = 0u;
    } else if (*cnt >= CTRL_DEBOUNCE_MS) {
        *state = sample;
        *cnt = 0u;
    } else {
        (*cnt)++;
    }
}

static void rail_aux_5v_enable(bool en)
{
    bsp_gpio_write(GPIOC, GPIO_PIN_12, en ? BSP_GPIO_HIGH : BSP_GPIO_LOW);
}

static void rail_12v_enable(bool en)
{
    bsp_gpio_write(GPIOB, GPIO_PIN_4, en ? BSP_GPIO_LOW : BSP_GPIO_HIGH);
}

static void rail_28v_m12v_enable(bool en)
{
    bsp_gpio_write(GPIOB, GPIO_PIN_5, en ? BSP_GPIO_LOW : BSP_GPIO_HIGH);
}

static void apply_outputs(bool aux5, bool v12, bool v28_m12)
{
    /* Soft turn-off order: rear/output rails first, then front auxiliary/5V rail. */
    if ((!v12) || (!v28_m12)) {
        rail_12v_enable(false);
        rail_28v_m12v_enable(false);
    }

    rail_aux_5v_enable(aux5);

    if (v12) {
        rail_12v_enable(true);
    }
    if (v28_m12) {
        rail_28v_m12v_enable(true);
    }
}

void power_ctrl_init(void)
{
    /* All GPIO directions and safe defaults are configured in bsp_gpio_init(). */
    g_ga_id = 0u;
    if (bsp_gpio_read(GPIOC, GPIO_PIN_7) == BSP_GPIO_HIGH) g_ga_id |= 0x01u;
    if (bsp_gpio_read(GPIOC, GPIO_PIN_8) == BSP_GPIO_HIGH) g_ga_id |= 0x02u;
    if (bsp_gpio_read(GPIOA, GPIO_PIN_8) == BSP_GPIO_HIGH) g_ga_id |= 0x04u;
    g_ipmb_addr_8bit = (uint8_t)(IPMB_PM_BASE_ADDR_8BIT + (g_ga_id << 1));

    g_en_active = input_low_active(GPIOB, GPIO_PIN_13);
    g_inh_active = input_low_active(GPIOB, GPIO_PIN_14);
    g_nvmro_allows_update = input_low_active(GPIOB, GPIO_PIN_9);
    g_en_cnt = 0u;
    g_inh_cnt = 0u;
    g_nvmro_cnt = 0u;

    apply_outputs(false, false, false);
    g_power_state = POWER_STATE_OFF;
}

void power_ctrl_task_1ms(void)
{
    debounce_bool(input_low_active(GPIOB, GPIO_PIN_13), &g_en_cnt, &g_en_active);
    debounce_bool(input_low_active(GPIOB, GPIO_PIN_14), &g_inh_cnt, &g_inh_active);
    debounce_bool(input_low_active(GPIOB, GPIO_PIN_9), &g_nvmro_cnt, &g_nvmro_allows_update);

    if ((!g_nvmro_allows_update) || (!g_en_active)) {
        apply_outputs(false, false, false);
        g_power_state = POWER_STATE_OFF;
        return;
    }

    if (g_inh_active) {
        apply_outputs(true, false, true);
        g_power_state = POWER_STATE_INHIBIT;
    } else {
        apply_outputs(true, true, true);
        g_power_state = POWER_STATE_ON;
    }
}

void power_ctrl_force_off(void)
{
    apply_outputs(false, false, false);
    g_power_state = POWER_STATE_FAULT;
}

power_state_t power_ctrl_get_state(void)
{
    return g_power_state;
}

bool power_ctrl_enable_active(void)
{
    return g_en_active;
}

bool power_ctrl_inhibit_active(void)
{
    return g_inh_active;
}

bool power_ctrl_nvmro_allows_update(void)
{
    return g_nvmro_allows_update;
}

bool power_ctrl_input_voltage_ok(void)
{
    /* PWOUT_TEST_MCU: low = normal, high = abnormal. */
    return (bsp_gpio_read(GPIOB, GPIO_PIN_8) == BSP_GPIO_LOW) ? true : false;
}

uint8_t power_ctrl_get_ga_id(void)
{
    return g_ga_id;
}

uint8_t power_ctrl_get_ipmb_addr_8bit(void)
{
    return g_ipmb_addr_8bit;
}
