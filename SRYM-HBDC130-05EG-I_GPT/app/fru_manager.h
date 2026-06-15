/**
 * @file    fru_manager.h
 * @brief   IPMI FRU 镜像生成和分段读取接口。
 *          当前实现生成公共头、板卡信息区和产品信息区，并向 Storage NetFn 命令处理层
 *          提供 FRU 总长度和按偏移读取能力。
 * @author  GPT
 * @date    2026-06-15
 * @version V0.4
 */
#ifndef FRU_MANAGER_H
#define FRU_MANAGER_H

#include "app_config.h"
#include <stdint.h>

/**
 * @brief  在 RAM 中生成完整 FRU 镜像并计算各区域校验和。
 * @param  无。
 * @retval 无。
 */
void fru_manager_init(void);

/**
 * @brief  获取当前 FRU 镜像的有效总长度。
 * @param  无。
 * @retval FRU 有效字节数。
 */
uint16_t fru_get_area_size(void);

/**
 * @brief  从 FRU 镜像中读取指定片段。
 * @param  offset 起始偏移，单位字节。
 * @param  count  请求读取字节数。
 * @param  out    输出缓冲区。
 * @retval 实际读取字节数；失败时返回 0。
 */
uint8_t fru_read(uint16_t offset, uint8_t count, uint8_t *out);

#endif
