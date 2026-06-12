#include "bsp_flash.h"
#include "gd32f10X_fmc.h"

#define BSP_FLASH_PAGE_SIZE      0x00000800u

static bool flash_addr_valid(uint32_t addr)
{
    return ((addr >= BSP_FLASH_PARAM_BASE) && (addr < (BSP_FLASH_PARAM_BASE + BSP_FLASH_PARAM_SIZE))) ? true : false;
}

bool bsp_flash_erase_page(uint32_t addr)
{
    uint32_t page_addr;
    FMC_State state;

    if (!flash_addr_valid(addr)) return false;
    page_addr = addr & ~(BSP_FLASH_PAGE_SIZE - 1u);
    if (!flash_addr_valid(page_addr)) return false;

    FMC_Unlock();
    state = FMC_ErasePage(page_addr);
    FMC_Lock();

    return (state == FMC_READY) ? true : false;
}

bool bsp_flash_write_words(uint32_t addr, const uint32_t *data, uint32_t word_count)
{
    uint32_t i;
    FMC_State state;

    if ((data == 0) || (word_count == 0u)) return false;
    if ((addr & 0x03u) != 0u) return false;
    if (!flash_addr_valid(addr)) return false;
    if (!flash_addr_valid(addr + (word_count * 4u) - 1u)) return false;

    FMC_Unlock();
    for (i = 0u; i < word_count; i++) {
        state = FMC_ProgramWord(addr + (i * 4u), data[i]);
        if (state != FMC_READY) {
            FMC_Lock();
            return false;
        }
        if (*(volatile uint32_t *)(addr + (i * 4u)) != data[i]) {
            FMC_Lock();
            return false;
        }
    }
    FMC_Lock();
    return true;
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
