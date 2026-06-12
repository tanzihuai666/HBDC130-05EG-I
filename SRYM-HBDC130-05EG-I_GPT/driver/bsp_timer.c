/* File: bsp_timer.c
 * Module: 1ms SysTick time base.
 */
#include "bsp_timer.h"
#include "gd32f10x.h"

/* Millisecond counter increased by SysTick ISR. */
static volatile uint32_t g_bsp_ms;

/* SysTick ISR, called every 1ms after bsp_timer_init_1ms(). */
void SysTick_Handler(void)
{
    g_bsp_ms++;
}

/* Init SysTick to 1ms period. */
void bsp_timer_init_1ms(void)
{
    g_bsp_ms = 0u;
    SysTick_Config(SystemCoreClock / 1000u);
}

/* Return current millisecond counter. */
uint32_t bsp_millis(void)
{
    return g_bsp_ms;
}

/* Simple CPU hold loop based on millisecond counter. */
void bsp_delay_ms(uint32_t ms)
{
    uint32_t start = bsp_millis();
    while ((uint32_t)(bsp_millis() - start) < ms) {
    }
}
