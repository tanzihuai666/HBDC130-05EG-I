/* File: bsp_timer.h
 * Module: 1ms tick interface.
 */
#ifndef BSP_TIMER_H
#define BSP_TIMER_H

#include <stdint.h>

/* Init 1ms tick. */
void bsp_timer_init_1ms(void);

/* Return current ms count. */
uint32_t bsp_millis(void);

/* Hold CPU for ms count. */
void bsp_delay_ms(uint32_t ms);

#endif
