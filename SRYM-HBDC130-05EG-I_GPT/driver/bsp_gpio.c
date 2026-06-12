/**
 * @file    bsp_gpio.c
 * @brief   GPIO 板级初始化与基础读写封装。
 *          统一管理电源控制、GA 地址、FAIL*、I2C 收发器使能、SYNC/SYSRESET 预留引脚。
 * @author  GPT
 * @date    2026-06-12
 * @version V0.3
 */
#include "bsp_gpio.h"

/** @brief 写 GPIO 电平。@param port GPIO 端口。@param pin GPIO 引脚。@param level 输出电平。@retval 无。 */
void bsp_gpio_write(GPIO_TypeDef *port, uint16_t pin, bsp_gpio_level_t level)
{
    if (level == BSP_GPIO_HIGH) {
        GPIO_SetBits(port, pin);
    } else {
        GPIO_ResetBits(port, pin);
    }
}

/** @brief 读 GPIO 电平。@param port GPIO 端口。@param pin GPIO 引脚。@retval BSP_GPIO_HIGH/BSP_GPIO_LOW。 */
bsp_gpio_level_t bsp_gpio_read(GPIO_TypeDef *port, uint16_t pin)
{
    return (GPIO_ReadInputBit(port, pin) != RESET) ? BSP_GPIO_HIGH : BSP_GPIO_LOW;
}

/** @brief 控制 FAIL* 输出。@param fault_active true=故障有效，PC6 输出低。@retval 无。 */
void bsp_gpio_set_fail(bool fault_active)
{
    bsp_gpio_write(GPIOC, GPIO_PIN_6, fault_active ? BSP_GPIO_LOW : BSP_GPIO_HIGH);
}

/** @brief 控制 I2C1 收发器使能。@param enable true=使能，PD2 输出低。@retval 无。 */
void bsp_gpio_set_i2c1_transceiver_enable(bool enable)
{
    bsp_gpio_write(GPIOD, GPIO_PIN_2, enable ? BSP_GPIO_LOW : BSP_GPIO_HIGH);
}

/** @brief 控制预留 I2C2 收发器使能。@param enable true=使能，PB12 输出低。@retval 无。 */
void bsp_gpio_set_i2c2_transceiver_enable(bool enable)
{
    bsp_gpio_write(GPIOB, GPIO_PIN_12, enable ? BSP_GPIO_LOW : BSP_GPIO_HIGH);
}

/** @brief 初始化本项目所有 GPIO。@param 无。@retval 无。 */
void bsp_gpio_init(void)
{
    GPIO_InitPara gpio;

    RCC_APB2PeriphClock_Enable(RCC_APB2PERIPH_GPIOA |
                               RCC_APB2PERIPH_GPIOB |
                               RCC_APB2PERIPH_GPIOC |
                               RCC_APB2PERIPH_GPIOD |
                               RCC_APB2PERIPH_AF, ENABLE);

    gpio.GPIO_Speed = GPIO_SPEED_50MHZ;

    /* PB8/PB9/PB13/PB14/PB15：输入信号，PB15 SYSRESET* 功能暂不实现，仅配置安全输入态。 */
    gpio.GPIO_Mode = GPIO_MODE_IN_FLOATING;
    gpio.GPIO_Pin = GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15;
    GPIO_Init(GPIOB, &gpio);

    /* GA[2:0]：用于计算 IPMB 地址。 */
    gpio.GPIO_Pin = GPIO_PIN_7 | GPIO_PIN_8;
    GPIO_Init(GPIOC, &gpio);
    gpio.GPIO_Pin = GPIO_PIN_8;
    GPIO_Init(GPIOA, &gpio);

    /* PA11 SYNC_IN：同步功能暂不实现，仅配置安全输入态。 */
    gpio.GPIO_Pin = GPIO_PIN_11;
    GPIO_Init(GPIOA, &gpio);

    /* 电源控制输出和预留 SYNC_OUT。 */
    gpio.GPIO_Mode = GPIO_MODE_OUT_PP;
    gpio.GPIO_Pin = GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_12;
    GPIO_Init(GPIOB, &gpio);
    gpio.GPIO_Pin = GPIO_PIN_6 | GPIO_PIN_12;
    GPIO_Init(GPIOC, &gpio);
    gpio.GPIO_Pin = GPIO_PIN_12;
    GPIO_Init(GPIOA, &gpio);

    /* I2C 收发器使能脚。 */
    gpio.GPIO_Pin = GPIO_PIN_2;
    GPIO_Init(GPIOD, &gpio);
    gpio.GPIO_Pin = GPIO_PIN_12;
    GPIO_Init(GPIOB, &gpio);

    /* 安全默认态：输出电源关闭、FAIL* 释放、I2C1 收发器使能。 */
    bsp_gpio_write(GPIOC, GPIO_PIN_12, BSP_GPIO_LOW);
    bsp_gpio_write(GPIOB, GPIO_PIN_4, BSP_GPIO_HIGH);
    bsp_gpio_write(GPIOB, GPIO_PIN_5, BSP_GPIO_HIGH);
    bsp_gpio_set_fail(false);
    bsp_gpio_set_i2c1_transceiver_enable(true);
    bsp_gpio_set_i2c2_transceiver_enable(false);
}
