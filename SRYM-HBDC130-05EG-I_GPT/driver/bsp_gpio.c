/**
 * @file    bsp_gpio.c
 * @brief   项目相关 GPIO 的统一初始化和基础读写实现。
 *          本模块集中管理电源控制输入输出、GA 地址输入、FAIL 输出、I2C 外部收发器
 *          使能以及暂未实现功能所占用的 SYNC、SYSRESET 引脚。其他业务模块不得重复
 *          修改这些 GPIO 的模式，只能通过本文件提供的接口读写电平。
 * @author  GPT
 * @date    2026-06-15
 * @version V0.4
 */
#include "bsp_gpio.h"

/**
 * @brief  设置指定 GPIO 引脚的输出电平。
 * @param  port  GPIO 端口，例如 GPIOA、GPIOB。
 * @param  pin   GPIO 引脚位掩码，例如 GPIO_PIN_6。
 * @param  level 目标逻辑电平。
 * @retval 无。
 */
void bsp_gpio_write(GPIO_TypeDef *port,
                    uint16_t pin,
                    bsp_gpio_level_t level)
{
    if (level == BSP_GPIO_HIGH) {
        /* GPIO_SetBits() 通过位设置寄存器输出高电平。 */
        GPIO_SetBits(port, pin);
    } else {
        /* GPIO_ResetBits() 通过位复位寄存器输出低电平。 */
        GPIO_ResetBits(port, pin);
    }
}

/**
 * @brief  读取指定 GPIO 引脚的当前输入电平。
 * @param  port GPIO 端口。
 * @param  pin  GPIO 引脚位掩码。
 * @retval BSP_GPIO_HIGH=高电平；BSP_GPIO_LOW=低电平。
 */
bsp_gpio_level_t bsp_gpio_read(GPIO_TypeDef *port, uint16_t pin)
{
    if (GPIO_ReadInputBit(port, pin) != RESET) {
        return BSP_GPIO_HIGH;
    }

    return BSP_GPIO_LOW;
}

/**
 * @brief  控制低电平有效的 FAIL 输出。
 * @param  fault_active true=存在故障，PC6 输出低电平；false=无故障，PC6 输出高电平。
 * @retval 无。
 */
void bsp_gpio_set_fail(bool fault_active)
{
    bsp_gpio_write(GPIOC,
                   GPIO_PIN_6,
                   fault_active ? BSP_GPIO_LOW : BSP_GPIO_HIGH);
}

/**
 * @brief  控制 I2C1 外部收发器使能。
 * @param  enable true=使能收发器，PD2 输出低电平；false=关闭，PD2 输出高电平。
 * @retval 无。
 */
void bsp_gpio_set_i2c1_transceiver_enable(bool enable)
{
    bsp_gpio_write(GPIOD,
                   GPIO_PIN_2,
                   enable ? BSP_GPIO_LOW : BSP_GPIO_HIGH);
}

/**
 * @brief  控制预留 I2C2 外部收发器使能。
 * @param  enable true=使能收发器，PB12 输出低电平；false=关闭，PB12 输出高电平。
 * @retval 无。
 * @note   当前软件只实现 I2C1/IPMB，I2C2 保持关闭。
 */
void bsp_gpio_set_i2c2_transceiver_enable(bool enable)
{
    bsp_gpio_write(GPIOB,
                   GPIO_PIN_12,
                   enable ? BSP_GPIO_LOW : BSP_GPIO_HIGH);
}

/**
 * @brief  初始化本项目使用的全部 GPIO，并设置上电安全默认电平。
 * @param  无。
 * @retval 无。
 */
void bsp_gpio_init(void)
{
    GPIO_InitPara gpio;

    /*
     * 打开 GPIOA、GPIOB、GPIOC、GPIOD 和复用功能控制器时钟。
     * AF 时钟同时供 USART、I2C 等复用引脚配置使用。
     */
    RCC_APB2PeriphClock_Enable(RCC_APB2PERIPH_GPIOA |
                               RCC_APB2PERIPH_GPIOB |
                               RCC_APB2PERIPH_GPIOC |
                               RCC_APB2PERIPH_GPIOD |
                               RCC_APB2PERIPH_AF,
                               ENABLE);

    /* 输出引脚统一采用 50MHz 速度；输入模式下该字段不会影响电气特性。 */
    gpio.GPIO_Speed = GPIO_SPEED_50MHZ;

    /*
     * PB8  ：PWOUT_TEST_MCU 输入；
     * PB9  ：NVMRO 输入；
     * PB13 ：EN 输入；
     * PB14 ：INH 输入；
     * PB15 ：SYSRESET 输入。
     * SYSRESET 功能按用户要求暂不实现，仅保持浮空输入，不主动驱动总线。
     */
    gpio.GPIO_Mode = GPIO_MODE_IN_FLOATING;
    gpio.GPIO_Pin = GPIO_PIN_8 |
                    GPIO_PIN_9 |
                    GPIO_PIN_13 |
                    GPIO_PIN_14 |
                    GPIO_PIN_15;
    GPIO_Init(GPIOB, &gpio);

    /* PC7=GA0、PC8=GA1，均配置为浮空输入。 */
    gpio.GPIO_Pin = GPIO_PIN_7 | GPIO_PIN_8;
    GPIO_Init(GPIOC, &gpio);

    /* PA8=GA2，配置为浮空输入。 */
    gpio.GPIO_Pin = GPIO_PIN_8;
    GPIO_Init(GPIOA, &gpio);

    /* PA11=SYNC_IN，同步功能暂不实现，只配置为安全输入状态。 */
    gpio.GPIO_Pin = GPIO_PIN_11;
    GPIO_Init(GPIOA, &gpio);

    /*
     * PB4=INH2、PB5=INH1、PB12=I2C2 收发器使能，均为普通推挽输出。
     * PB12 在当前版本保持高电平，表示关闭 I2C2 收发器。
     */
    gpio.GPIO_Mode = GPIO_MODE_OUT_PP;
    gpio.GPIO_Pin = GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_12;
    GPIO_Init(GPIOB, &gpio);

    /* PC6=FAIL、PC12=EN1，配置为普通推挽输出。 */
    gpio.GPIO_Pin = GPIO_PIN_6 | GPIO_PIN_12;
    GPIO_Init(GPIOC, &gpio);

    /* PA12=SYNC_OUT，同步功能暂不实现，暂时配置为普通推挽输出。 */
    gpio.GPIO_Pin = GPIO_PIN_12;
    GPIO_Init(GPIOA, &gpio);

    /* PD2=I2C1 外部收发器低有效使能。 */
    gpio.GPIO_Pin = GPIO_PIN_2;
    GPIO_Init(GPIOD, &gpio);

    /*
     * 设置所有关键输出的上电安全状态：
     * PC12=0 关闭辅助/+5V；
     * PB4=1  禁止 +12V；
     * PB5=1  禁止 +28V/-12V；
     * PC6=1  释放低有效 FAIL；
     * PD2=0  使能 I2C1 收发器；
     * PB12=1 关闭暂未使用的 I2C2 收发器。
     */
    bsp_gpio_write(GPIOC, GPIO_PIN_12, BSP_GPIO_LOW);
    bsp_gpio_write(GPIOB, GPIO_PIN_4, BSP_GPIO_HIGH);
    bsp_gpio_write(GPIOB, GPIO_PIN_5, BSP_GPIO_HIGH);
    bsp_gpio_set_fail(false);
    bsp_gpio_set_i2c1_transceiver_enable(true);
    bsp_gpio_set_i2c2_transceiver_enable(false);
}
