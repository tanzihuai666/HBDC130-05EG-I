/**
 * @file    power_ctrl.h
 * @brief   电源控制状态机对外接口。
 *          提供电源控制初始化、1ms 周期任务、故障强制关断、输入状态查询以及 GA/IPMB
 *          地址读取功能。
 * @author  GPT
 * @date    2026-06-15
 * @version V0.4
 */
#ifndef POWER_CTRL_H
#define POWER_CTRL_H

#include "app_config.h"
#include <stdint.h>
#include <stdbool.h>

/**
 * @brief 电源控制状态机状态。
 */
typedef enum {
    POWER_STATE_OFF = 0,     /* EN 或 NVMRO 条件不满足，全部受控电源轨关闭。 */
    POWER_STATE_STANDBY,     /* 预留待机状态，当前版本尚未单独使用。 */
    POWER_STATE_ON,          /* EN、NVMRO 有效且 INH 无效，全部电源轨开启。 */
    POWER_STATE_INHIBIT,     /* INH 有效，按需求关闭部分电源轨。 */
    POWER_STATE_FAULT        /* 严重故障触发，全部电源轨被强制关断。 */
} power_state_t;

/**
 * @brief  初始化输入稳定状态、GA 地址、IPMB 地址和电源输出。
 * @param  无。
 * @retval 无。
 */
void power_ctrl_init(void);

/**
 * @brief  1ms 周期电源控制任务，完成输入去抖、状态判断和输出更新。
 * @param  无。
 * @retval 无。
 */
void power_ctrl_task_1ms(void);

/**
 * @brief  故障保护触发时强制关闭全部受控电源轨。
 * @param  无。
 * @retval 无。
 */
void power_ctrl_force_off(void);

/**
 * @brief  获取当前电源状态机状态。
 * @param  无。
 * @retval 当前 power_state_t 状态。
 */
power_state_t power_ctrl_get_state(void);

/**
 * @brief  获取去抖后的 EN 输入状态。
 * @param  无。
 * @retval true=EN 有效；false=EN 无效。
 */
bool power_ctrl_enable_active(void);

/**
 * @brief  获取去抖后的 INH 输入状态。
 * @param  无。
 * @retval true=INH 有效；false=INH 无效。
 */
bool power_ctrl_inhibit_active(void);

/**
 * @brief  获取去抖后的 NVMRO 允许状态。
 * @param  无。
 * @retval true=允许运行或更新；false=不允许。
 */
bool power_ctrl_nvmro_allows_update(void);

/**
 * @brief  获取 PB8 PWOUT_TEST_MCU 输入电压检测状态。
 * @param  无。
 * @retval true=输入电压正常；false=输入电压异常。
 */
bool power_ctrl_input_voltage_ok(void);

/**
 * @brief  获取 GA[2:0] 组合得到的槽位号。
 * @param  无。
 * @retval 0~7 的槽位号。
 */
uint8_t power_ctrl_get_ga_id(void);

/**
 * @brief  获取根据 GA 地址计算得到的本机 8 位 IPMB 地址。
 * @param  无。
 * @retval 本机 8 位 IPMB 地址。
 */
uint8_t power_ctrl_get_ipmb_addr_8bit(void);

#endif
