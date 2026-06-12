#include "bsp_gpio.h"

void bsp_gpio_write(GPIO_TypeDef *port, uint16_t pin, bsp_gpio_level_t level)
{
    if (level == BSP_GPIO_HIGH) {
        GPIO_SetBits(port, pin);
    } else {
        GPIO_ResetBits(port, pin);
    }
}

bsp_gpio_level_t bsp_gpio_read(GPIO_TypeDef *port, uint16_t pin)
{
    return (GPIO_ReadInputBit(port, pin) != RESET) ? BSP_GPIO_HIGH : BSP_GPIO_LOW;
}

void bsp_gpio_set_fail(bool fault_active)
{
    /* FAIL-MCU: PC6, low active. */
    bsp_gpio_write(GPIOC, GPIO_PIN_6, fault_active ? BSP_GPIO_LOW : BSP_GPIO_HIGH);
}

void bsp_gpio_set_i2c1_transceiver_enable(bool enable)
{
    /* I2C1 enable: PD2, low active. */
    bsp_gpio_write(GPIOD, GPIO_PIN_2, enable ? BSP_GPIO_LOW : BSP_GPIO_HIGH);
}

void bsp_gpio_set_i2c2_transceiver_enable(bool enable)
{
    /* I2C2 enable: PB12, low active, reserved. */
    bsp_gpio_write(GPIOB, GPIO_PIN_12, enable ? BSP_GPIO_LOW : BSP_GPIO_HIGH);
}

void bsp_gpio_init(void)
{
    GPIO_InitPara gpio;

    RCC_APB2PeriphClock_Enable(RCC_APB2PERIPH_GPIOA |
                               RCC_APB2PERIPH_GPIOB |
                               RCC_APB2PERIPH_GPIOC |
                               RCC_APB2PERIPH_GPIOD |
                               RCC_APB2PERIPH_AF, ENABLE);

    gpio.GPIO_Speed = GPIO_SPEED_50MHZ;

    /* Control inputs: PB8 PWOUT_TEST, PB9 NVMRO, PB13 EN, PB14 INH, PB15 SYSRESET default input. */
    gpio.GPIO_Mode = GPIO_MODE_IN_FLOATING;
    gpio.GPIO_Pin = GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15;
    GPIO_Init(GPIOB, &gpio);

    /* GA inputs: PC7/PC8/PA8. */
    gpio.GPIO_Pin = GPIO_PIN_7 | GPIO_PIN_8;
    GPIO_Init(GPIOC, &gpio);
    gpio.GPIO_Pin = GPIO_PIN_8;
    GPIO_Init(GPIOA, &gpio);

    /* SYNC input PA11. Logic is reserved for later; pin is only made safe here. */
    gpio.GPIO_Pin = GPIO_PIN_11;
    GPIO_Init(GPIOA, &gpio);

    /* Power outputs: PB4 INH2, PB5 INH1, PC12 EN1, PC6 FAIL, PA12 SYNC_OUT. */
    gpio.GPIO_Mode = GPIO_MODE_OUT_PP;
    gpio.GPIO_Pin = GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_12;
    GPIO_Init(GPIOB, &gpio);
    gpio.GPIO_Pin = GPIO_PIN_6 | GPIO_PIN_12;
    GPIO_Init(GPIOC, &gpio);
    gpio.GPIO_Pin = GPIO_PIN_12;
    GPIO_Init(GPIOA, &gpio);

    /* I2C transceiver enable pins. */
    gpio.GPIO_Pin = GPIO_PIN_2;
    GPIO_Init(GPIOD, &gpio);
    gpio.GPIO_Pin = GPIO_PIN_12;
    GPIO_Init(GPIOB, &gpio);

    /* Safe output defaults. */
    bsp_gpio_write(GPIOC, GPIO_PIN_12, BSP_GPIO_LOW);   /* EN1 off */
    bsp_gpio_write(GPIOB, GPIO_PIN_4, BSP_GPIO_HIGH);   /* +12V inhibited */
    bsp_gpio_write(GPIOB, GPIO_PIN_5, BSP_GPIO_HIGH);   /* +28V/-12V inhibited */
    bsp_gpio_set_fail(false);
    bsp_gpio_set_i2c1_transceiver_enable(true);
    bsp_gpio_set_i2c2_transceiver_enable(false);
}
