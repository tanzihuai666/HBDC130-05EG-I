/**
 * @file    bsp_gpio.h
 * @brief   GPIO 板级抽象接口。
 * @author  GPT
 * @date    2026-06-12
 * @version V0.3
 */
#ifndef BSP_GPIO_H
#define BSP_GPIO_H

#include "gd32f10x.h"
#include <stdint.h>
#include <stdbool.h>

/** @brief GPIO 电平枚举。 */
typedef enum {
    BSP_GPIO_LOW = 0,   /* 低电平。 */
    BSP_GPIO_HIGH = 1   /* 高电平。 */
} bsp_gpio_level_t;

/** @brief 初始化所有项目相关 GPIO。@param 无。@retval 无。 */
void bsp_gpio_init(void);

/** @brief 写 GPIO 电平。@param port 端口。@param pin 引脚。@param level 电平。@retval 无。 */
void bsp_gpio_write(GPIO_TypeDef *port, uint16_t pin, bsp_gpio_level_t level);

/** @brief 读 GPIO 电平。@param port 端口。@param pin 引脚。@retval GPIO 电平。 */
bsp_gpio_level_t bsp_gpio_read(GPIO_TypeDef *port, uint16_t pin);

/** @brief 设置 FAIL* 输出。@param fault_active true=故障有效，FAIL* 输出低。@retval 无。 */
void bsp_gpio_set_fail(bool fault_active);

/** @brief 使能/关闭 I2C1 收发器。@param enable true=使能。@retval 无。 */
void bsp_gpio_set_i2c1_transceiver_enable(bool enable);

/** @brief 使能/关闭 I2C2 收发器。@param enable true=使能。@retval 无。 */
void bsp_gpio_set_i2c2_transceiver_enable(bool enable);

#endif
