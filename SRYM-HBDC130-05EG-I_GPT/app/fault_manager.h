/**
 * @file    fault_manager.h
 * @brief   故障检测与保护联动接口。
 *          提供故障管理初始化、10ms 周期检测、活动故障查询和故障字读取功能。
 * @author  GPT
 * @date    2026-06-15
 * @version V0.4
 */
#ifndef FAULT_MANAGER_H
#define FAULT_MANAGER_H

#include "app_config.h"
#include <stdint.h>
#include <stdbool.h>

/**
 * @brief  清零故障状态并释放低有效 FAIL 输出。
 * @param  无。
 * @retval 无。
 */
void fault_manager_init(void);

/**
 * @brief  每 10ms 汇总电压、温度和 PWOUT_TEST 状态，并执行故障保护联动。
 * @param  无。
 * @retval 无。
 */
void fault_manager_task_10ms(void);

/**
 * @brief  查询当前是否存在任意活动故障。
 * @param  无。
 * @retval true=存在活动故障；false=无活动故障。
 */
bool fault_manager_has_active_fault(void);

/**
 * @brief  获取当前 32 位故障字。
 * @param  无。
 * @retval 故障位集合。
 */
uint32_t fault_manager_get_bits(void);

#endif
