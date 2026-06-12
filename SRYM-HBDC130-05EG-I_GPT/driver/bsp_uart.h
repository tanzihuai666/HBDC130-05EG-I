#ifndef BSP_UART_H
#define BSP_UART_H

#include "gd32f10x.h"
#include <stdint.h>

void bsp_uart1_init(uint32_t baud);
void bsp_uart1_putc(uint8_t ch);
void bsp_uart1_write(const uint8_t *buf, uint16_t len);

#endif
