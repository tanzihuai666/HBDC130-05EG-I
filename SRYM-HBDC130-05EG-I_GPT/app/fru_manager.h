/**
 * @file    fru_manager.h
 * @brief   IPMI FRU 信息区生成与读取接口。
 * @author  GPT
 * @date    2026-06-12
 * @version V0.3
 */
#ifndef FRU_MANAGER_H
#define FRU_MANAGER_H

#include "app_config.h"
#include <stdint.h>

/** @brief 生成 FRU Common Header、Board Area、Product Area。@param 无。@retval 无。 */
void fru_manager_init(void);

/** @brief 获取 FRU 总长度。@param 无。@retval FRU 字节数。 */
uint16_t fru_get_area_size(void);

/** @brief 分段读取 FRU 数据。@param offset 偏移。@param count 请求长度。@param out 输出缓存。@retval 实际读取字节数。 */
uint8_t fru_read(uint16_t offset, uint8_t count, uint8_t *out);

#endif
