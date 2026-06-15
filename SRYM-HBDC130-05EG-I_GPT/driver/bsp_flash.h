/**
 * @file    bsp_flash.h
 * @brief   片内 Flash 参数区访问接口。
 *          参数区用于后续保存标定参数、序列号、故障记录等非易失数据，所有接口都会限制
 *          访问范围，防止覆盖应用程序代码。
 * @author  GPT
 * @date    2026-06-15
 * @version V0.4
 */
#ifndef BSP_FLASH_H
#define BSP_FLASH_H

#include "app_config.h"
#include <stdint.h>
#include <stdbool.h>

/*
 * 参数区位于 256KB Flash 的最后 16KB：0x0803C000~0x0803FFFF。
 * 链接脚本和应用代码大小必须保证不占用该区域。
 */
#define BSP_FLASH_PARAM_BASE      0x0803C000u
#define BSP_FLASH_PARAM_SIZE      0x00004000u

/**
 * @brief  擦除指定地址所在的参数区 Flash 页。
 * @param  addr 参数区内任意绝对地址。
 * @retval true=擦除成功；false=地址非法或 FMC 擦除失败。
 */
bool bsp_flash_erase_page(uint32_t addr);

/**
 * @brief  以 32 位字为单位写入参数区，并逐字回读校验。
 * @param  addr       目标绝对地址，必须 4 字节对齐。
 * @param  data       待写入 32 位数据数组。
 * @param  word_count 待写入字数量。
 * @retval true=全部写入并校验成功；false=失败。
 */
bool bsp_flash_write_words(uint32_t addr,
                           const uint32_t *data,
                           uint32_t word_count);

/**
 * @brief  计算标准 CRC32，用于参数块完整性校验。
 * @param  data 数据指针。
 * @param  len  数据长度，单位字节。
 * @retval CRC32 结果。
 */
uint32_t bsp_flash_crc32(const uint8_t *data, uint32_t len);

#endif
