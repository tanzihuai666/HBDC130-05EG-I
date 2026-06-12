#ifndef POWER_CTRL_H
#define POWER_CTRL_H

#include "app_config.h"
#include <stdint.h>
#include <stdbool.h>

typedef enum {
    POWER_STATE_OFF = 0,
    POWER_STATE_STANDBY,
    POWER_STATE_ON,
    POWER_STATE_INHIBIT,
    POWER_STATE_FAULT
} power_state_t;

void power_ctrl_init(void);
void power_ctrl_task_1ms(void);
void power_ctrl_force_off(void);
power_state_t power_ctrl_get_state(void);
bool power_ctrl_enable_active(void);
bool power_ctrl_nvmro_allows_update(void);
uint8_t power_ctrl_get_ga_id(void);
uint8_t power_ctrl_get_ipmb_addr_8bit(void);

#endif
