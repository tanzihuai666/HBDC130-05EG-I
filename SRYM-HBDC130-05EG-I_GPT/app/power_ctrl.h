/**
 * @file    power_ctrl.h
 * @brief   电源控制应用层接口。
 * @author  GPT
 * @date    2026-06-12
 * @version V0.3
 */
#ifndef POWER_CTRL_H
#define POWER_CTRL_H

#include "app_config.h"
#include <stdint.h>
#include <stdbool.h>

/**
 * @brief 电源状态机状态枚举。
 */
typedef enum {
    POWER_STATE_OFF = 0,     /* 全部电源轨关闭。 */
    POWER_STATE_STANDBY,     /* 预留待机状态，当前未单独使用。 */
    POWER_STATE_ON,          /* 全部受控电源轨开启。 */
    POWER_STATE_INHIBIT,     /* INH 有效，部分电源轨关闭。 */
    POWER_STATE_FAULT        /* 故障保护触发，强制关断。 */
} power_state_t;

/** @brief 初始化电源控制模块。@param 无。@retval 无。 */
void power_ctrl_init(void);

/** @brief 1ms 电源控制任务，包含输入去抖和输出更新。@param 无。@retval 无。 */
void power_ctrl_task_1ms(void);

/** @brief 故障联动强制关断所有受控电源轨。@param 无。@retval 无。 */
void power_ctrl_force_off(void);

/** @brief 获取当前电源状态。@param 无。@retval power_state_t 当前状态。 */
power_state_t power_ctrl_get_state(void);

/** @brief 获取 EN 输入是否有效。@param 无。@retval true=有效。 */
bool power_ctrl_enable_active(void);

/** @brief 获取 INH 输入是否有效。@param 无。@retval true=有效。 */
bool power_ctrl_inhibit_active(void);

/** @brief 获取 NVMRO 是否允许运行/更新。@param 无。@retval true=允许。 */
bool power_ctrl_nvmro_allows_update(void);

/** @brief 获取 PB8 PWOUT_TEST_MCU 输入状态。@param 无。@retval true=输入电压正常。 */
bool power_ctrl_input_voltage_ok(void);

/** @brief 获取 GA[2:0] 计算得到的槽位 ID。@param 无。@retval 0~7。 */
uint8_t power_ctrl_get_ga_id(void);

/** @brief 获取电源模块 IPMB 8-bit 地址。@param 无。@retval IPMB 地址。 */
uint8_t power_ctrl_get_ipmb_addr_8bit(void);

#endif
