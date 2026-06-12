#ifndef BSP_FLASH_H
#define BSP_FLASH_H

#include "gd32f10x.h"
#include <stdint.h>
#include <stdbool.h>

#define BSP_FLASH_PARAM_BASE      0x0803C000u
#define BSP_FLASH_PARAM_SIZE      0x00004000u

bool bsp_flash_erase_page(uint32_t addr);
bool bsp_flash_write_words(uint32_t addr, const uint32_t *data, uint32_t word_count);
uint32_t bsp_flash_crc32(const uint8_t *data, uint32_t len);

#endif
