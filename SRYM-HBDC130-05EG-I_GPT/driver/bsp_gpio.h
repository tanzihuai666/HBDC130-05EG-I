/**
 * @file    bsp_gpio.h
 * @brief   项目相关 GPIO 的板级抽象接口。
 *          业务层通过本接口访问 GPIO 电平和专用控制信号，避免在多个模块中重复使用
 *          库函数直接配置同一引脚，造成模式或默认电平冲突。
 * @author  GPT
 * @date    2026-06-15
 * @version V0.4
 */
#ifndef BSP_GPIO_H
#define BSP_GPIO_H

#include "gd32f10x.h"
#include <stdint.h>
#include <stdbool.h>

/**
 * @brief GPIO 逻辑电平枚举。
 */
typedef enum {
    BSP_GPIO_LOW = 0,   /* 逻辑低电平。 */
    BSP_GPIO_HIGH = 1   /* 逻辑高电平。 */
} bsp_gpio_level_t;

/**
 * @brief  初始化全部项目相关 GPIO 并设置上电安全默认电平。
 * @param  无。
 * @retval 无。
 */
void bsp_gpio_init(void);

/**
 * @brief  设置指定 GPIO 引脚的输出电平。
 * @param  port  GPIO 端口。
 * @param  pin   GPIO 引脚位掩码。
 * @param  level 目标电平。
 * @retval 无。
 */
void bsp_gpio_write(GPIO_TypeDef *port,
                    uint16_t pin,
                    bsp_gpio_level_t level);

/**
 * @brief  读取指定 GPIO 引脚的当前电平。
 * @param  port GPIO 端口。
 * @param  pin  GPIO 引脚位掩码。
 * @retval BSP_GPIO_HIGH 或 BSP_GPIO_LOW。
 */
bsp_gpio_level_t bsp_gpio_read(GPIO_TypeDef *port, uint16_t pin);

/**
 * @brief  控制低电平有效的 FAIL 输出。
 * @param  fault_active true=故障有效；false=释放 FAIL。
 * @retval 无。
 */
void bsp_gpio_set_fail(bool fault_active);

/**
 * @brief  控制低电平有效的 I2C1 外部收发器使能。
 * @param  enable true=使能；false=关闭。
 * @retval 无。
 */
void bsp_gpio_set_i2c1_transceiver_enable(bool enable);

/**
 * @brief  控制低电平有效的预留 I2C2 外部收发器使能。
 * @param  enable true=使能；false=关闭。
 * @retval 无。
 */
void bsp_gpio_set_i2c2_transceiver_enable(bool enable);

#endif
