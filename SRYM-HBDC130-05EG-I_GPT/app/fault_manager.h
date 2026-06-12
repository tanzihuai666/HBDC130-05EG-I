#ifndef FAULT_MANAGER_H
#define FAULT_MANAGER_H

#include "app_config.h"
#include <stdint.h>
#include <stdbool.h>

void fault_manager_init(void);
void fault_manager_task_10ms(void);
bool fault_manager_has_active_fault(void);
uint32_t fault_manager_get_bits(void);

#endif
