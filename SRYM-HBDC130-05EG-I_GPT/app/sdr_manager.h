#ifndef SDR_MANAGER_H
#define SDR_MANAGER_H

#include "app_config.h"
#include <stdint.h>

void sdr_manager_init(uint8_t owner_addr_8bit, uint8_t entity_instance);
uint8_t sdr_get_count(void);
uint16_t sdr_reserve(void);
uint8_t sdr_read_record(uint16_t record_id, uint8_t offset, uint8_t count, uint8_t *next_lsb, uint8_t *next_msb, uint8_t *out);

#endif
