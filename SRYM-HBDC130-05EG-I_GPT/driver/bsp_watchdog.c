/**
 * @file    bsp_watchdog.c
 * @brief   Independent watchdog BSP.
 * @author  GPT
 * @date    2026-06-12
 * @version V0.3
 */
#include "bsp_watchdog.h"
#include "app_config.h"
#include "gd32f10x.h"

/** @brief Initialize IWDG to about 1s timeout. */
void bsp_watchdog_init(void)
{
    IWDG_Write_Enable(IWDG_WRITEACCESS_ENABLE);
    IWDG_SetPrescaler(IWDG_PRESCALER_64);
    IWDG_SetReloadValue(625u); /* About 1s at 40kHz/64. */
    IWDG_ReloadCounter();
    IWDG_Enable();
    APP_LOGI("watchdog init");
}

/** @brief Feed IWDG; called only after main loop tasks return. */
void bsp_watchdog_feed(void)
{
    IWDG_ReloadCounter();
}
