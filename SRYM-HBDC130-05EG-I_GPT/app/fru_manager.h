#ifndef FRU_MANAGER_H
#define FRU_MANAGER_H

#include "app_config.h"
#include <stdint.h>

void fru_manager_init(void);
uint16_t fru_get_area_size(void);
uint8_t fru_read(uint16_t offset, uint8_t count, uint8_t *out);

#endif
