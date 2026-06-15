/**
 * @file    app_config.h
 * @brief   HBDC130-05EG-I 应用层全局配置文件。
 *          本文件集中定义固件版本、日志等级、IPMB/IPMI 协议常量、FRU/SDR 容量及
 *          传感器事件位。所有应用层和 BSP 层模块均应优先引用本文件中的统一定义，
 *          避免同一参数在不同源文件中重复定义而产生不一致。
 * @author  GPT
 * @date    2026-06-15
 * @version V0.4
 */
#ifndef APP_CONFIG_H
#define APP_CONFIG_H

#include "gd32f10x.h"
#include <stdio.h>

/*
 * 固件版本号。
 * Get Device ID 命令会直接返回下面两个 BCD 字节：
 * 0x02 表示主版本 2，0x00 表示次版本 0。
 */
#define APP_FW_MAJOR_BCD                 0x02u
#define APP_FW_MINOR_BCD                 0x00u

/*
 * 串口日志配置。
 * APP_LOG_ENABLE 为 0 时，所有日志宏在编译阶段被消除，不占用运行时间和串口带宽。
 * APP_LOG_LEVEL 数值越大，输出越详细。
 */
#define APP_LOG_ENABLE                   1u
#define APP_LOG_LEVEL_ERROR              1u
#define APP_LOG_LEVEL_WARN               2u
#define APP_LOG_LEVEL_INFO               3u
#define APP_LOG_LEVEL_DEBUG              4u
#define APP_LOG_LEVEL                    APP_LOG_LEVEL_INFO

/*
 * 日志换行统一只输出 '\n'。
 * fputc() 重定向函数负责将 '\n' 转换成串口终端需要的 "\r\n"，从而避免出现重复回车
 * "\r\r\n"，也避免不同模块自行拼接换行符导致日志排版不一致。
 */
#if APP_LOG_ENABLE
#define APP_LOG_RAW(...)                 printf(__VA_ARGS__)
#define APP_LOGE(...)                    do { if (APP_LOG_LEVEL >= APP_LOG_LEVEL_ERROR) { printf("[E] "); printf(__VA_ARGS__); printf("\n"); } } while (0)
#define APP_LOGW(...)                    do { if (APP_LOG_LEVEL >= APP_LOG_LEVEL_WARN)  { printf("[W] "); printf(__VA_ARGS__); printf("\n"); } } while (0)
#define APP_LOGI(...)                    do { if (APP_LOG_LEVEL >= APP_LOG_LEVEL_INFO)  { printf("[I] "); printf(__VA_ARGS__); printf("\n"); } } while (0)
#define APP_LOGD(...)                    do { if (APP_LOG_LEVEL >= APP_LOG_LEVEL_DEBUG) { printf("[D] "); printf(__VA_ARGS__); printf("\n"); } } while (0)
#else
#define APP_LOG_RAW(...)                 do { } while (0)
#define APP_LOGE(...)                    do { } while (0)
#define APP_LOGW(...)                    do { } while (0)
#define APP_LOGI(...)                    do { } while (0)
#define APP_LOGD(...)                    do { } while (0)
#endif

/*
 * IPMB 地址采用 8 位表示形式，即 7 位从地址左移 1 位。
 * 模块最终地址 = IPMB_PM_BASE_ADDR_8BIT + (GA_ID << 1)。
 */
#define IPMB_PM_BASE_ADDR_8BIT           0x72u   /* 电源模块基础地址。 */
#define IPMB_BMC_ADDR_8BIT               0x20u   /* BMC 常用地址，仅作为调试参考。 */
#define IPMB_MAX_FRAME_LEN               64u     /* 完整 IPMB 帧最大缓存长度。 */
#define IPMB_MAX_RSP_DATA_LEN            48u     /* IPMI 响应数据区最大长度。 */
#define IPMB_MAX_READ_CHUNK              32u     /* FRU/SDR 单次最大读取字节数。 */
#define IPMB_TX_QUEUE_DEPTH              4u      /* IPMB 响应发送队列深度。 */
#define IPMB_TX_RETRY_MAX                3u      /* 仲裁丢失或 NACK 后最大重试次数。 */

/* IPMI NetFn：本项目最小实现所使用的三个功能组。 */
#define IPMI_NETFN_SENSOR_EVENT          0x04u
#define IPMI_NETFN_APP                   0x06u
#define IPMI_NETFN_STORAGE               0x0Au

/* IPMI 命令码：与 V2 实现方案要求保持一致。 */
#define IPMI_CMD_GET_DEVICE_ID           0x01u
#define IPMI_CMD_GET_SELF_TEST           0x04u
#define IPMI_CMD_GET_DEVICE_SDR_INFO     0x20u
#define IPMI_CMD_GET_DEVICE_SDR          0x21u
#define IPMI_CMD_RESERVE_DEVICE_SDR      0x22u
#define IPMI_CMD_GET_SENSOR_THRESH       0x27u
#define IPMI_CMD_GET_SENSOR_READING      0x2Du
#define IPMI_CMD_GET_SENSOR_TYPE         0x2Fu
#define IPMI_CMD_GET_FRU_INFO            0x10u
#define IPMI_CMD_READ_FRU_DATA           0x11u
#define IPMI_CMD_WRITE_FRU_DATA          0x12u

/* IPMI Completion Code。 */
#define IPMI_CC_OK                       0x00u   /* 命令执行成功。 */
#define IPMI_CC_INVALID_CMD              0xC1u   /* 命令不支持。 */
#define IPMI_CC_REQ_LEN_INVALID          0xC7u   /* 请求数据长度错误。 */
#define IPMI_CC_PARAM_OUT_OF_RANGE       0xC9u   /* 参数越界。 */
#define IPMI_CC_NOT_PRESENT              0xCBu   /* 请求对象不存在。 */
#define IPMI_CC_INVALID_FIELD            0xCCu   /* 请求字段无效。 */
#define IPMI_CC_NOT_SUPPORTED_STATE      0xD5u   /* 当前状态不允许执行。 */

/* FRU、SDR 和传感器数据库规模。 */
#define FRU_DEVICE_ID_POWER_MODULE       0x00u
#define SENSOR_COUNT                     20u
#define SDR_RECORD_COUNT                 SENSOR_COUNT
#define SDR_MAX_RECORD_SIZE              64u
#define FRU_AREA_MAX_SIZE                512u

/* IPMI 传感器状态字节。 */
#define IPMI_SENSOR_STATUS_NORMAL        0xC0u   /* 扫描使能，读数有效。 */
#define IPMI_SENSOR_STATUS_UNAVAILABLE   0x20u   /* 读数暂不可用。 */

/* Get Sensor Reading 返回的阈值断言事件位。 */
#define IPMI_EVT_LNC_ASSERT              (1u << 0)
#define IPMI_EVT_LC_ASSERT               (1u << 1)
#define IPMI_EVT_LNR_ASSERT              (1u << 2)
#define IPMI_EVT_UNC_ASSERT              (1u << 3)
#define IPMI_EVT_UC_ASSERT               (1u << 4)
#define IPMI_EVT_UNR_ASSERT              (1u << 5)

#endif
