#include "bsp_watchdog.h"
#include "gd32f10x.h"

void bsp_watchdog_init(void)
{
    IWDG_Write_Enable(IWDG_WRITEACCESS_ENABLE);
    IWDG_SetPrescaler(IWDG_PRESCALER_64);
    IWDG_SetReloadValue(625u); /* About 1s at 40kHz/64. */
    IWDG_ReloadCounter();
    IWDG_Enable();
}

void bsp_watchdog_feed(void)
{
    IWDG_ReloadCounter();
}
