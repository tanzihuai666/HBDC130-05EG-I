#ifndef BSP_TIMER_H
#define BSP_TIMER_H

#include <stdint.h>

void bsp_timer_init_1ms(void);
uint32_t bsp_millis(void);
void bsp_delay_ms(uint32_t ms);

#endif
