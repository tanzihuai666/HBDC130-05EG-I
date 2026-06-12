/**
 * @file    bsp_flash.h
 * @brief   Internal Flash parameter area interface.
 * @author  GPT
 * @date    2026-06-12
 * @version V0.3
 */
#ifndef BSP_FLASH_H
#define BSP_FLASH_H

#include "gd32f10x.h"
#include <stdint.h>
#include <stdbool.h>

/* Parameter area: last 16KB of 256KB application Flash. */
#define BSP_FLASH_PARAM_BASE      0x0803C000u
#define BSP_FLASH_PARAM_SIZE      0x00004000u

/** @brief Erase the page containing addr. @param addr address inside parameter area. @retval true on success. */
bool bsp_flash_erase_page(uint32_t addr);

/** @brief Program 32-bit words into Flash. @param addr target address. @param data source words. @param word_count word count. @retval true on success. */
bool bsp_flash_write_words(uint32_t addr, const uint32_t *data, uint32_t word_count);

/** @brief Calculate CRC32. @param data byte buffer. @param len byte length. @retval CRC32 value. */
uint32_t bsp_flash_crc32(const uint8_t *data, uint32_t len);

#endif
