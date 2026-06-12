#include "bsp_i2c.h"
#include "bsp_gpio.h"
#include <string.h>

static bsp_i2c_rx_callback_t g_rx_cb;
static uint8_t g_rx_buf[BSP_I2C_RX_MAX];
static volatile uint8_t g_rx_len;

void bsp_i2c1_register_rx_callback(bsp_i2c_rx_callback_t cb)
{
    g_rx_cb = cb;
}

void bsp_i2c1_ipmb_init(uint8_t own_addr_7bit)
{
    GPIO_InitPara gpio;
    NVIC_InitPara nvic;

    (void)own_addr_7bit;

    RCC_APB2PeriphClock_Enable(RCC_APB2PERIPH_GPIOB | RCC_APB2PERIPH_AF, ENABLE);
    RCC_APB1PeriphClock_Enable(RCC_APB1PERIPH_I2C1, ENABLE);

    /* PB6 SCL, PB7 SDA, open-drain AF, 400 kbit/s target. */
    gpio.GPIO_Pin = GPIO_PIN_6 | GPIO_PIN_7;
    gpio.GPIO_Speed = GPIO_SPEED_50MHZ;
    gpio.GPIO_Mode = GPIO_MODE_AF_OD;
    GPIO_Init(GPIOB, &gpio);

    bsp_gpio_set_i2c1_transceiver_enable(true);

    nvic.NVIC_IRQ = I2C1_EV_IRQn;
    nvic.NVIC_IRQPreemptPriority = 3u;
    nvic.NVIC_IRQSubPriority = 0u;
    nvic.NVIC_IRQEnable = ENABLE;
    NVIC_Init(&nvic);

    nvic.NVIC_IRQ = I2C1_ER_IRQn;
    nvic.NVIC_IRQPreemptPriority = 2u;
    nvic.NVIC_IRQSubPriority = 0u;
    nvic.NVIC_IRQEnable = ENABLE;
    NVIC_Init(&nvic);

    g_rx_len = 0u;

    /* TODO after Keil package check:
     * 1. Configure I2C1 as 7-bit slave at own_addr_7bit.
     * 2. Enable ACK, ADDR/RBNE/STPDET/BERR/ARLO/OVR interrupts.
     * 3. Enable 400 kbit/s master mode for response writes.
     *
     * The wrapper is kept here to isolate GD32 SPL naming differences.
     */
}

bool bsp_i2c1_master_write(uint8_t dest_addr_7bit, const uint8_t *data, uint8_t len)
{
    (void)dest_addr_7bit;
    (void)data;
    (void)len;

    /* TODO: implement multi-master transmit with ARLO/NACK retry once SPL naming is verified. */
    return false;
}

void bsp_i2c1_recover_bus(void)
{
    uint8_t i;
    GPIO_InitPara gpio;

    /* Disable I2C peripheral externally through transceiver, then clock SCL manually. */
    bsp_gpio_set_i2c1_transceiver_enable(false);

    gpio.GPIO_Pin = GPIO_PIN_6 | GPIO_PIN_7;
    gpio.GPIO_Speed = GPIO_SPEED_50MHZ;
    gpio.GPIO_Mode = GPIO_MODE_OUT_OD;
    GPIO_Init(GPIOB, &gpio);

    for (i = 0u; i < 9u; i++) {
        GPIO_ResetBits(GPIOB, GPIO_PIN_6);
        for (volatile uint32_t d = 0u; d < 200u; d++) { }
        GPIO_SetBits(GPIOB, GPIO_PIN_6);
        for (volatile uint32_t d = 0u; d < 200u; d++) { }
    }

    bsp_gpio_set_i2c1_transceiver_enable(true);
}

void I2C1_EV_IRQHandler(void)
{
    /* TODO: use GD32 I2C event flags to collect slave RX bytes into g_rx_buf.
     * On STOP condition call g_rx_cb(g_rx_buf, g_rx_len).
     */
    if ((g_rx_cb != 0) && (g_rx_len > 0u)) {
        g_rx_cb(g_rx_buf, g_rx_len);
        g_rx_len = 0u;
    }
}

void I2C1_ER_IRQHandler(void)
{
    /* TODO: clear BERR/ARLO/OVR/NACK flags according to GD32 SPL names. */
    g_rx_len = 0u;
}
