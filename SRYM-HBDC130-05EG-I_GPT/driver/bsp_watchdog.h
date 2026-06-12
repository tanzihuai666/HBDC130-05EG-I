/**
 * @file    bsp_watchdog.h
 * @brief   Independent watchdog interface.
 * @author  GPT
 * @date    2026-06-12
 * @version V0.3
 */
#ifndef BSP_WATCHDOG_H
#define BSP_WATCHDOG_H

/** @brief Initialize IWDG. @param None. @retval None. */
void bsp_watchdog_init(void);

/** @brief Reload IWDG counter. @param None. @retval None. */
void bsp_watchdog_feed(void);

#endif
