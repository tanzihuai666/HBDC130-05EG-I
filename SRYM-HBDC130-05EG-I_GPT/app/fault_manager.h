/**
 * @file    fault_manager.h
 * @brief   故障管理模块接口。
 * @author  GPT
 * @date    2026-06-12
 * @version V0.3
 */
#ifndef FAULT_MANAGER_H
#define FAULT_MANAGER_H

#include "app_config.h"
#include <stdint.h>
#include <stdbool.h>

/** @brief 初始化故障管理模块，默认释放 FAIL*。@param 无。@retval 无。 */
void fault_manager_init(void);

/** @brief 10ms 周期故障检测与保护联动。@param 无。@retval 无。 */
void fault_manager_task_10ms(void);

/** @brief 查询是否有活动故障。@param 无。@retval true=有故障。 */
bool fault_manager_has_active_fault(void);

/** @brief 获取当前故障位集合。@param 无。@retval 32-bit 故障位。 */
uint32_t fault_manager_get_bits(void);

#endif
