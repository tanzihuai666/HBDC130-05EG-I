#ifndef BSP_GPIO_H
#define BSP_GPIO_H

#include "gd32f10x.h"
#include <stdint.h>
#include <stdbool.h>

typedef enum {
    BSP_GPIO_LOW = 0,
    BSP_GPIO_HIGH = 1
} bsp_gpio_level_t;

void bsp_gpio_init(void);
void bsp_gpio_write(GPIO_TypeDef *port, uint16_t pin, bsp_gpio_level_t level);
bsp_gpio_level_t bsp_gpio_read(GPIO_TypeDef *port, uint16_t pin);
void bsp_gpio_set_fail(bool fault_active);
void bsp_gpio_set_i2c1_transceiver_enable(bool enable);
void bsp_gpio_set_i2c2_transceiver_enable(bool enable);

#endif
