#ifndef IPMB_DRIVER_H
#define IPMB_DRIVER_H

#include "app_config.h"
#include <stdint.h>

void ipmb_driver_init(uint8_t own_addr_8bit);
void ipmb_driver_task(void);
uint32_t ipmb_driver_get_rx_count(void);
uint32_t ipmb_driver_get_tx_count(void);
uint32_t ipmb_driver_get_drop_count(void);

#endif
