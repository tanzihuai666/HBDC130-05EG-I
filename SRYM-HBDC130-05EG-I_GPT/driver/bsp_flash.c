/**
 * @file    bsp_flash.c
 * @brief   片内 Flash 参数区擦除、写入和 CRC32 计算实现。
 *          本模块只允许访问应用程序末尾预留的参数区，避免误擦除启动代码和主程序。
 *          每次写入后都会执行回读校验，用于发现编程失败、电源不稳或地址配置错误。
 * @author  GPT
 * @date    2026-06-15
 * @version V0.4
 */
#include "bsp_flash.h"
#include "gd32f10X_fmc.h"

/* GD32F103RCT6 高密度器件单页大小为 2KB。 */
#define BSP_FLASH_PAGE_SIZE      0x00000800u

/**
 * @brief  判断地址是否位于预留参数区内。
 * @param  addr 待检查的绝对 Flash 地址。
 * @retval true=地址合法；false=地址超出参数区。
 */
static bool flash_address_is_valid(uint32_t addr)
{
    uint32_t param_end;

    param_end = BSP_FLASH_PARAM_BASE + BSP_FLASH_PARAM_SIZE;

    return ((addr >= BSP_FLASH_PARAM_BASE) && (addr < param_end)) ? true : false;
}

/**
 * @brief  擦除指定地址所在的参数区 Flash 页。
 * @param  addr 参数区内任意地址，函数会自动向下对齐到页首地址。
 * @retval true=擦除成功；false=地址非法或 FMC 返回错误。
 */
bool bsp_flash_erase_page(uint32_t addr)
{
    uint32_t page_addr;
    FMC_State state;

    if (!flash_address_is_valid(addr)) {
        APP_LOGE("Flash erase rejected: address=0x%08lX", (unsigned long)addr);
        return false;
    }

    /* 页大小为 2KB，因此清除低 11 位得到页首地址。 */
    page_addr = addr & ~(BSP_FLASH_PAGE_SIZE - 1u);

    if (!flash_address_is_valid(page_addr)) {
        APP_LOGE("Flash erase rejected: page=0x%08lX", (unsigned long)page_addr);
        return false;
    }

    /* FMC 写保护解除后执行页擦除，完成后立即重新上锁。 */
    FMC_Unlock();
    state = FMC_ErasePage(page_addr);
    FMC_Lock();

    if (state != FMC_READY) {
        APP_LOGE("Flash erase failed: page=0x%08lX state=%u",
                 (unsigned long)page_addr,
                 (unsigned int)state);
        return false;
    }

    APP_LOGI("Flash erase completed: page=0x%08lX",
             (unsigned long)page_addr);
    return true;
}

/**
 * @brief  以 32 位字为单位向参数区写入数据，并逐字回读校验。
 * @param  addr       目标绝对地址，必须 4 字节对齐。
 * @param  data       待写入的 32 位数据数组。
 * @param  word_count 待写入的 32 位字数量。
 * @retval true=全部写入并校验成功；false=参数非法、FMC 编程失败或回读不一致。
 */
bool bsp_flash_write_words(uint32_t addr,
                           const uint32_t *data,
                           uint32_t word_count)
{
    uint32_t index;
    uint32_t last_addr;
    FMC_State state;

    if ((data == 0) || (word_count == 0u)) {
        APP_LOGE("Flash write rejected: empty input");
        return false;
    }

    if ((addr & 0x03u) != 0u) {
        APP_LOGE("Flash write rejected: unaligned address=0x%08lX",
                 (unsigned long)addr);
        return false;
    }

    /* 计算最后一个写入字节地址，并确认整个写入范围都位于参数区内。 */
    last_addr = addr + (word_count * 4u) - 1u;
    if ((!flash_address_is_valid(addr)) ||
        (!flash_address_is_valid(last_addr))) {
        APP_LOGE("Flash write rejected: range=0x%08lX~0x%08lX",
                 (unsigned long)addr,
                 (unsigned long)last_addr);
        return false;
    }

    FMC_Unlock();

    for (index = 0u; index < word_count; index++) {
        uint32_t current_addr;

        current_addr = addr + (index * 4u);
        state = FMC_ProgramWord(current_addr, data[index]);

        if (state != FMC_READY) {
            FMC_Lock();
            APP_LOGE("Flash program failed: index=%lu state=%u",
                     (unsigned long)index,
                     (unsigned int)state);
            return false;
        }

        /* 立即回读刚写入的字，防止静默编程失败。 */
        if (*(volatile uint32_t *)current_addr != data[index]) {
            FMC_Lock();
            APP_LOGE("Flash verify failed: address=0x%08lX",
                     (unsigned long)current_addr);
            return false;
        }
    }

    FMC_Lock();

    APP_LOGI("Flash write completed: address=0x%08lX words=%lu",
             (unsigned long)addr,
             (unsigned long)word_count);
    return true;
}

/**
 * @brief  计算标准反射式 CRC32。
 * @param  data 待计算数据指针。
 * @param  len  数据长度，单位字节。
 * @retval CRC32 结果；data 为空时返回 0。
 * @note   初值为 0xFFFFFFFF，多项式为 0xEDB88320，最终结果按位取反。
 */
uint32_t bsp_flash_crc32(const uint8_t *data, uint32_t len)
{
    uint32_t crc;
    uint32_t byte_index;
    uint8_t bit_index;

    if (data == 0) {
        return 0u;
    }

    crc = 0xFFFFFFFFu;

    for (byte_index = 0u; byte_index < len; byte_index++) {
        crc ^= data[byte_index];

        for (bit_index = 0u; bit_index < 8u; bit_index++) {
            if ((crc & 1u) != 0u) {
                crc = (crc >> 1) ^ 0xEDB88320u;
            } else {
                crc >>= 1;
            }
        }
    }

    return ~crc;
}
