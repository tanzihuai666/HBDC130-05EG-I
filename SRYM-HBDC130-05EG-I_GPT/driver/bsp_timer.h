/**
 * @file    bsp_timer.h
 * @brief   SysTick 1ms 系统时基接口。
 *          该接口为主循环周期任务调度和少量初始化延时提供统一时间基准。
 * @author  GPT
 * @date    2026-06-15
 * @version V0.4
 */
#ifndef BSP_TIMER_H
#define BSP_TIMER_H

#include <stdint.h>

/**
 * @brief  初始化 SysTick，使其每 1ms 产生一次中断。
 * @param  无。
 * @retval 无。
 */
void bsp_timer_init_1ms(void);

/**
 * @brief  获取系统运行毫秒数。
 * @param  无。
 * @retval 32 位毫秒计数值。
 */
uint32_t bsp_millis(void);

/**
 * @brief  执行阻塞式毫秒延时。
 * @param  ms 延时时间，单位 ms。
 * @retval 无。
 * @warning 不应在中断服务函数或主循环高频关键路径中调用。
 */
void bsp_delay_ms(uint32_t ms);

#endif
