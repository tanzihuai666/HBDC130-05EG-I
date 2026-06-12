#include "bsp_flash.h"

bool bsp_flash_erase_page(uint32_t addr)
{
    /* GD32F10x standard library Flash API names differ between package versions.
     * Keep this function isolated for package-specific adjustment after Keil build.
     */
    (void)addr;
    return false;
}

bool bsp_flash_write_words(uint32_t addr, const uint32_t *data, uint32_t word_count)
{
    (void)addr;
    (void)data;
    (void)word_count;
    return false;
}

uint32_t bsp_flash_crc32(const uint8_t *data, uint32_t len)
{
    uint32_t crc = 0xFFFFFFFFu;
    uint32_t i;
    uint8_t bit;

    if (data == 0) return 0u;

    for (i = 0u; i < len; i++) {
        crc ^= data[i];
        for (bit = 0u; bit < 8u; bit++) {
            if ((crc & 1u) != 0u) {
                crc = (crc >> 1) ^ 0xEDB88320u;
            } else {
                crc >>= 1;
            }
        }
    }
    return ~crc;
}
