#ifndef BSP_I2C_H
#define BSP_I2C_H

#include "gd32f10x.h"
#include <stdint.h>
#include <stdbool.h>

#define BSP_I2C_RX_MAX        64u
#define BSP_I2C_TX_MAX        64u

typedef void (*bsp_i2c_rx_callback_t)(const uint8_t *data, uint8_t len);

void bsp_i2c1_ipmb_init(uint8_t own_addr_7bit);
void bsp_i2c1_register_rx_callback(bsp_i2c_rx_callback_t cb);
bool bsp_i2c1_master_write(uint8_t dest_addr_7bit, const uint8_t *data, uint8_t len);
void bsp_i2c1_recover_bus(void);
void I2C1_EV_IRQHandler(void);
void I2C1_ER_IRQHandler(void);

#endif
