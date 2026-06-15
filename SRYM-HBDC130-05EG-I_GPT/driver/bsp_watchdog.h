/**
 * @file    bsp_watchdog.h
 * @brief   独立看门狗 IWDG 板级接口。
 * @author  GPT
 * @date    2026-06-15
 * @version V0.4
 */
#ifndef BSP_WATCHDOG_H
#define BSP_WATCHDOG_H

/**
 * @brief  配置并启动独立看门狗。
 * @param  无。
 * @retval 无。
 */
void bsp_watchdog_init(void);

/**
 * @brief  重装独立看门狗计数器。
 * @param  无。
 * @retval 无。
 */
void bsp_watchdog_feed(void);

#endif
