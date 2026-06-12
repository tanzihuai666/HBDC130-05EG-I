#include "power_ctrl.h"

/*
 * GPIO mapping from software requirement:
 * EN input      PB13, MCU side low active after inverter
 * INH input     PB14, MCU side low active after inverter
 * NVMRO input   PB9,  MCU side low active after inverter
 * EN1 output    PC12, high enables +3.3V_AUX/+5V
 * INH2 output   PB4,  low enables +12V
 * INH1 output   PB5,  low enables +28V/-12V
 * FAIL output   PC6,  low active
 * GA0/GA1/GA2   PC7/PC8/PA8
 */

static power_state_t g_power_state = POWER_STATE_OFF;
static uint8_t g_ga_id;
static uint8_t g_ipmb_addr_8bit = IPMB_PM_BASE_ADDR_8BIT;

static bool gpio_read_bit(GPIO_TypeDef *port, uint16_t pin)
{
    return (GPIO_ReadInputBit(port, pin) != RESET) ? true : false;
}

static void rail_aux_5v_enable(bool en)
{
    if (en) GPIO_SetBits(GPIOC, GPIO_PIN_12);
    else GPIO_ResetBits(GPIOC, GPIO_PIN_12);
}

static void rail_12v_enable(bool en)
{
    if (en) GPIO_ResetBits(GPIOB, GPIO_PIN_4);
    else GPIO_SetBits(GPIOB, GPIO_PIN_4);
}

static void rail_28v_m12v_enable(bool en)
{
    if (en) GPIO_ResetBits(GPIOB, GPIO_PIN_5);
    else GPIO_SetBits(GPIOB, GPIO_PIN_5);
}

static bool read_en_active(void)
{
    return gpio_read_bit(GPIOB, GPIO_PIN_13) ? false : true;
}

static bool read_inh_active(void)
{
    return gpio_read_bit(GPIOB, GPIO_PIN_14) ? false : true;
}

bool power_ctrl_nvmro_allows_update(void)
{
    return gpio_read_bit(GPIOB, GPIO_PIN_9) ? false : true;
}

static void apply_outputs(bool aux5, bool v12, bool v28_m12)
{
    if ((!v12) || (!v28_m12)) {
        rail_12v_enable(false);
        rail_28v_m12v_enable(false);
    }
    rail_aux_5v_enable(aux5);
    if (v12) rail_12v_enable(true);
    if (v28_m12) rail_28v_m12v_enable(true);
}

void power_ctrl_init(void)
{
    GPIO_InitPara gpio;

    RCC_APB2PeriphClock_Enable(RCC_APB2PERIPH_GPIOA | RCC_APB2PERIPH_GPIOB | RCC_APB2PERIPH_GPIOC | RCC_APB2PERIPH_AF, ENABLE);

    gpio.GPIO_Speed = GPIO_SPEED_50MHZ;
    gpio.GPIO_Mode = GPIO_MODE_IN_FLOATING;
    gpio.GPIO_Pin = GPIO_PIN_9 | GPIO_PIN_13 | GPIO_PIN_14;
    GPIO_Init(GPIOB, &gpio);
    gpio.GPIO_Pin = GPIO_PIN_7 | GPIO_PIN_8;
    GPIO_Init(GPIOC, &gpio);
    gpio.GPIO_Pin = GPIO_PIN_8;
    GPIO_Init(GPIOA, &gpio);

    gpio.GPIO_Mode = GPIO_MODE_OUT_PP;
    gpio.GPIO_Pin = GPIO_PIN_4 | GPIO_PIN_5;
    GPIO_Init(GPIOB, &gpio);
    gpio.GPIO_Pin = GPIO_PIN_6 | GPIO_PIN_12;
    GPIO_Init(GPIOC, &gpio);

    g_ga_id = 0u;
    if (gpio_read_bit(GPIOC, GPIO_PIN_7)) g_ga_id |= 0x01u;
    if (gpio_read_bit(GPIOC, GPIO_PIN_8)) g_ga_id |= 0x02u;
    if (gpio_read_bit(GPIOA, GPIO_PIN_8)) g_ga_id |= 0x04u;
    g_ipmb_addr_8bit = (uint8_t)(IPMB_PM_BASE_ADDR_8BIT + (g_ga_id << 1));

    GPIO_SetBits(GPIOC, GPIO_PIN_6);
    apply_outputs(false, false, false);
    g_power_state = POWER_STATE_OFF;
}

void power_ctrl_task_1ms(void)
{
    bool en = read_en_active();
    bool inh = read_inh_active();
    bool nvmro = power_ctrl_nvmro_allows_update();

    if ((nvmro == false) || (en == false)) {
        apply_outputs(false, false, false);
        g_power_state = POWER_STATE_OFF;
        return;
    }

    if (inh) {
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
    return read_en_active();
}

uint8_t power_ctrl_get_ga_id(void)
{
    return g_ga_id;
}

uint8_t power_ctrl_get_ipmb_addr_8bit(void)
{
    return g_ipmb_addr_8bit;
}
