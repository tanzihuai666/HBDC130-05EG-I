#include "bsp_timer.h"
#include "gd32f10x.h"

static volatile uint32_t g_bsp_ms;

void SysTick_Handler(void)
{
    g_bsp_ms++;
}

void bsp_timer_init_1ms(void)
{
    g_bsp_ms = 0u;
    SysTick_Config(SystemCoreClock / 1000u);
}

uint32_t bsp_millis(void)
{
    return g_bsp_ms;
}

void bsp_delay_ms(uint32_t ms)
{
    uint32_t start = bsp_millis();
    while ((uint32_t)(bsp_millis() - start) < ms) {
    }
}
