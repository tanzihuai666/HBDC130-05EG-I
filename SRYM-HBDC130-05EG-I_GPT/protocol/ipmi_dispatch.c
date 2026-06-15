/**
 * @file    ipmi_dispatch.c
 * @brief   IPMI 命令分发和最小命令集处理实现。
 *          本模块接收已经通过 IPMB 校验和检查的 Request，根据 NetFn 和 Cmd 分发到
 *          Application、Sensor/Event、Storage 三组处理函数，并填写统一 Response 结构。
 *          本项目仅实现 V2 方案要求的最小命令集，不支持的命令统一返回 C1h。
 * @author  GPT
 * @date    2026-06-15
 * @version V0.4
 */
#include "ipmi_dispatch.h"
#include "sensor_manager.h"
#include "sdr_manager.h"
#include "fru_manager.h"
#include "fault_manager.h"

/* 本机 8 位 IPMB 地址，用于初始化 SDR Sensor Owner 字段。 */
static uint8_t g_own_addr_8bit;

/* GA[2:0] 计算得到的槽位号，用于生成 Entity Instance。 */
static uint8_t g_ga_id;

/**
 * @brief  将 16 位整数按小端顺序写入字节数组。
 * @param  output 目标缓冲区，至少包含 2 字节空间。
 * @param  value  待写入的 16 位数值。
 * @retval 固定返回 2，表示写入了 2 字节。
 */
static uint8_t ipmi_put_u16_le(uint8_t *output, uint16_t value)
{
    output[0] = (uint8_t)(value & 0xFFu);
    output[1] = (uint8_t)(value >> 8);
    return 2u;
}

/**
 * @brief  初始化 IPMI 命令处理所依赖的数据模块。
 * @param  own_addr_8bit 本机 8 位 IPMB 地址。
 * @param  ga_id         GA[2:0] 槽位号，范围 0~7。
 * @retval 无。
 */
void ipmi_init(uint8_t own_addr_8bit, uint8_t ga_id)
{
    g_own_addr_8bit = own_addr_8bit;
    g_ga_id = ga_id;

    /* 先初始化传感器表，FRU 和 SDR 构造过程会读取其中的描述信息。 */
    sensor_manager_init();
    fru_manager_init();

    /* Entity Instance 使用 GA_ID+1，避免实例号为 0。 */
    sdr_manager_init(g_own_addr_8bit, (uint8_t)(g_ga_id + 1u));

    APP_LOGI("IPMI initialized: own=0x%02X ga=%u",
             (unsigned int)own_addr_8bit,
             (unsigned int)ga_id);
}

/**
 * @brief  处理 Get Device ID 命令。
 * @param  rsp 响应结构输出指针。
 * @retval IPMI_CC_OK。
 */
static uint8_t ipmi_cmd_get_device_id(ipmi_response_t *rsp)
{
    uint32_t manufacturer_id;
    uint16_t product_id;

    /* Manufacturer ID 尚未取得正式 IANA PEN，当前使用 000000h 占位。 */
    manufacturer_id = 0u;

    /* Product ID 使用项目型号中的 1305 作为临时编码。 */
    product_id = 0x1305u;

    rsp->data[0] = 0x01u;                            /* Device ID。 */
    rsp->data[1] = 0x81u;                            /* Device Revision，设备可用。 */
    rsp->data[2] = APP_FW_MAJOR_BCD;                 /* Firmware Revision 1。 */
    rsp->data[3] = APP_FW_MINOR_BCD;                 /* Firmware Revision 2。 */
    rsp->data[4] = 0x02u;                            /* IPMI Version 2.0。 */
    rsp->data[5] = 0x29u;                            /* Additional Device Support。 */
    rsp->data[6] = (uint8_t)(manufacturer_id & 0xFFu);
    rsp->data[7] = (uint8_t)((manufacturer_id >> 8) & 0xFFu);
    rsp->data[8] = (uint8_t)((manufacturer_id >> 16) & 0xFFu);
    rsp->data[9] = (uint8_t)(product_id & 0xFFu);
    rsp->data[10] = (uint8_t)(product_id >> 8);
    rsp->data[11] = 0u;                              /* Auxiliary Firmware Revision。 */
    rsp->data[12] = 0u;
    rsp->data[13] = 0u;
    rsp->data[14] = 0u;
    rsp->data_len = 15u;

    return IPMI_CC_OK;
}

/**
 * @brief  处理 Get Self Test Results 命令。
 * @param  rsp 响应结构输出指针。
 * @retval IPMI_CC_OK。
 */
static uint8_t ipmi_cmd_get_self_test(ipmi_response_t *rsp)
{
    if (fault_manager_has_active_fault()) {
        /* 57h 表示存在设备相关错误，第二字节返回当前故障位低 8 位。 */
        rsp->data[0] = 0x57u;
        rsp->data[1] = (uint8_t)(fault_manager_get_bits() & 0xFFu);
    } else {
        /* 55h/00h 表示自检通过且无错误。 */
        rsp->data[0] = 0x55u;
        rsp->data[1] = 0x00u;
    }

    rsp->data_len = 2u;
    return IPMI_CC_OK;
}

/**
 * @brief  处理 Get Sensor Reading 命令。
 * @param  req 请求结构，数据区必须包含 1 字节 Sensor Number。
 * @param  rsp 响应结构输出指针。
 * @retval Completion Code。
 */
static uint8_t ipmi_cmd_get_sensor_reading(const ipmi_request_t *req,
                                           ipmi_response_t *rsp)
{
    const ipmi_sensor_t *sensor;

    if (req->data_len != 1u) {
        return IPMI_CC_REQ_LEN_INVALID;
    }

    sensor = sensor_get(req->data[0]);
    if (sensor == 0) {
        return IPMI_CC_NOT_PRESENT;
    }

    rsp->data[0] = sensor->raw_value;                        /* 8 位传感器读数。 */
    rsp->data[1] = sensor->sensor_status;                    /* 扫描/事件状态。 */
    rsp->data[2] = (uint8_t)(sensor->event_status & 0xFFu);  /* 事件位低字节。 */
    rsp->data[3] = (uint8_t)(sensor->event_status >> 8);     /* 事件位高字节。 */
    rsp->data_len = 4u;

    return IPMI_CC_OK;
}

/**
 * @brief  处理 Get Sensor Type 命令。
 * @param  req 请求结构，数据区必须包含 1 字节 Sensor Number。
 * @param  rsp 响应结构输出指针。
 * @retval Completion Code。
 */
static uint8_t ipmi_cmd_get_sensor_type(const ipmi_request_t *req,
                                        ipmi_response_t *rsp)
{
    const ipmi_sensor_t *sensor;

    if (req->data_len != 1u) {
        return IPMI_CC_REQ_LEN_INVALID;
    }

    sensor = sensor_get(req->data[0]);
    if (sensor == 0) {
        return IPMI_CC_NOT_PRESENT;
    }

    rsp->data[0] = sensor->sensor_type;
    rsp->data[1] = sensor->reading_type;
    rsp->data_len = 2u;

    return IPMI_CC_OK;
}

/**
 * @brief  处理 Get Sensor Thresholds 命令。
 * @param  req 请求结构，数据区必须包含 1 字节 Sensor Number。
 * @param  rsp 响应结构输出指针。
 * @retval Completion Code。
 */
static uint8_t ipmi_cmd_get_sensor_thresholds(const ipmi_request_t *req,
                                              ipmi_response_t *rsp)
{
    const ipmi_sensor_t *sensor;

    if (req->data_len != 1u) {
        return IPMI_CC_REQ_LEN_INVALID;
    }

    sensor = sensor_get(req->data[0]);
    if (sensor == 0) {
        return IPMI_CC_NOT_PRESENT;
    }

    /* 离散传感器没有模拟阈值，不响应本命令。 */
    if (sensor->kind == SENSOR_KIND_DISCRETE) {
        return IPMI_CC_INVALID_CMD;
    }

    rsp->data[0] = sensor->threshold_mask;
    rsp->data[1] = sensor->lnr;
    rsp->data[2] = sensor->lc;
    rsp->data[3] = sensor->lnc;
    rsp->data[4] = sensor->unc;
    rsp->data[5] = sensor->uc;
    rsp->data[6] = sensor->unr;
    rsp->data_len = 7u;

    return IPMI_CC_OK;
}

/**
 * @brief  处理 Get Device SDR Info 命令。
 * @param  rsp 响应结构输出指针。
 * @retval IPMI_CC_OK。
 */
static uint8_t ipmi_cmd_get_device_sdr_info(ipmi_response_t *rsp)
{
    rsp->data[0] = sdr_get_count();  /* 当前 SDR 记录数量。 */
    rsp->data[1] = 0x01u;            /* Dynamic Population，仅支持 Get SDR。 */
    rsp->data_len = 2u;

    return IPMI_CC_OK;
}

/**
 * @brief  处理 Reserve Device SDR Repository 命令。
 * @param  rsp 响应结构输出指针。
 * @retval IPMI_CC_OK。
 */
static uint8_t ipmi_cmd_reserve_device_sdr(ipmi_response_t *rsp)
{
    uint16_t reservation_id;

    reservation_id = sdr_reserve();
    ipmi_put_u16_le(rsp->data, reservation_id);
    rsp->data_len = 2u;

    return IPMI_CC_OK;
}

/**
 * @brief  处理 Get Device SDR 命令。
 * @param  req 请求数据格式：Reservation ID(2)、Record ID(2)、Offset(1)、BytesToRead(1)。
 * @param  rsp 响应结构输出指针。
 * @retval Completion Code。
 */
static uint8_t ipmi_cmd_get_device_sdr(const ipmi_request_t *req,
                                       ipmi_response_t *rsp)
{
    uint16_t record_id;
    uint8_t read_count;

    if (req->data_len != 6u) {
        return IPMI_CC_REQ_LEN_INVALID;
    }

    /* req->data[0:1] 为 Reservation ID；当前静态 SDR 数据库不强制校验该字段。 */
    record_id = (uint16_t)req->data[2] |
                ((uint16_t)req->data[3] << 8);

    /* 响应前 2 字节由 sdr_read_record() 填写 Next Record ID。 */
    read_count = sdr_read_record(record_id,
                                 req->data[4],
                                 req->data[5],
                                 &rsp->data[0],
                                 &rsp->data[1],
                                 &rsp->data[2]);

    if (read_count == 0u) {
        return IPMI_CC_PARAM_OUT_OF_RANGE;
    }

    rsp->data_len = (uint8_t)(read_count + 2u);
    return IPMI_CC_OK;
}

/**
 * @brief  处理 Get FRU Inventory Area Info 命令。
 * @param  req 请求数据区必须包含 1 字节 FRU Device ID。
 * @param  rsp 响应结构输出指针。
 * @retval Completion Code。
 */
static uint8_t ipmi_cmd_get_fru_info(const ipmi_request_t *req,
                                     ipmi_response_t *rsp)
{
    uint16_t fru_size;

    if (req->data_len != 1u) {
        return IPMI_CC_REQ_LEN_INVALID;
    }

    if (req->data[0] != FRU_DEVICE_ID_POWER_MODULE) {
        return IPMI_CC_NOT_PRESENT;
    }

    fru_size = fru_get_area_size();
    rsp->data[0] = (uint8_t)(fru_size & 0xFFu);
    rsp->data[1] = (uint8_t)(fru_size >> 8);
    rsp->data[2] = 0x00u;  /* bit0=0 表示按字节寻址。 */
    rsp->data_len = 3u;

    return IPMI_CC_OK;
}

/**
 * @brief  处理 Read FRU Data 命令。
 * @param  req 请求数据格式：FRU ID(1)、Offset LSB(1)、Offset MSB(1)、Count(1)。
 * @param  rsp 响应结构输出指针。
 * @retval Completion Code。
 */
static uint8_t ipmi_cmd_read_fru(const ipmi_request_t *req,
                                 ipmi_response_t *rsp)
{
    uint16_t offset;
    uint8_t read_count;

    if (req->data_len != 4u) {
        return IPMI_CC_REQ_LEN_INVALID;
    }

    if (req->data[0] != FRU_DEVICE_ID_POWER_MODULE) {
        return IPMI_CC_NOT_PRESENT;
    }

    offset = (uint16_t)req->data[1] |
             ((uint16_t)req->data[2] << 8);

    /* 响应 data[0] 为实际返回字节数，FRU 数据从 data[1] 开始。 */
    read_count = fru_read(offset, req->data[3], &rsp->data[1]);
    if (read_count == 0u) {
        return IPMI_CC_PARAM_OUT_OF_RANGE;
    }

    rsp->data[0] = read_count;
    rsp->data_len = (uint8_t)(read_count + 1u);

    return IPMI_CC_OK;
}

/**
 * @brief  分发 Application NetFn 命令。
 * @param  req 请求结构。
 * @param  rsp 响应结构。
 * @retval Completion Code。
 */
static uint8_t ipmi_dispatch_application(const ipmi_request_t *req,
                                         ipmi_response_t *rsp)
{
    switch (req->cmd) {
    case IPMI_CMD_GET_DEVICE_ID:
        if (req->data_len != 0u) {
            return IPMI_CC_REQ_LEN_INVALID;
        }
        return ipmi_cmd_get_device_id(rsp);

    case IPMI_CMD_GET_SELF_TEST:
        if (req->data_len != 0u) {
            return IPMI_CC_REQ_LEN_INVALID;
        }
        return ipmi_cmd_get_self_test(rsp);

    default:
        return IPMI_CC_INVALID_CMD;
    }
}

/**
 * @brief  分发 Sensor/Event NetFn 命令。
 * @param  req 请求结构。
 * @param  rsp 响应结构。
 * @retval Completion Code。
 */
static uint8_t ipmi_dispatch_sensor_event(const ipmi_request_t *req,
                                          ipmi_response_t *rsp)
{
    switch (req->cmd) {
    case IPMI_CMD_GET_DEVICE_SDR_INFO:
        if (req->data_len != 0u) {
            return IPMI_CC_REQ_LEN_INVALID;
        }
        return ipmi_cmd_get_device_sdr_info(rsp);

    case IPMI_CMD_RESERVE_DEVICE_SDR:
        if (req->data_len != 0u) {
            return IPMI_CC_REQ_LEN_INVALID;
        }
        return ipmi_cmd_reserve_device_sdr(rsp);

    case IPMI_CMD_GET_DEVICE_SDR:
        return ipmi_cmd_get_device_sdr(req, rsp);

    case IPMI_CMD_GET_SENSOR_THRESH:
        return ipmi_cmd_get_sensor_thresholds(req, rsp);

    case IPMI_CMD_GET_SENSOR_READING:
        return ipmi_cmd_get_sensor_reading(req, rsp);

    case IPMI_CMD_GET_SENSOR_TYPE:
        return ipmi_cmd_get_sensor_type(req, rsp);

    default:
        return IPMI_CC_INVALID_CMD;
    }
}

/**
 * @brief  分发 Storage NetFn 命令。
 * @param  req 请求结构。
 * @param  rsp 响应结构。
 * @retval Completion Code。
 */
static uint8_t ipmi_dispatch_storage(const ipmi_request_t *req,
                                     ipmi_response_t *rsp)
{
    switch (req->cmd) {
    case IPMI_CMD_GET_FRU_INFO:
        return ipmi_cmd_get_fru_info(req, rsp);

    case IPMI_CMD_READ_FRU_DATA:
        return ipmi_cmd_read_fru(req, rsp);

    case IPMI_CMD_WRITE_FRU_DATA:
        /* 当前版本 FRU 为只读，明确返回不支持命令。 */
        return IPMI_CC_INVALID_CMD;

    default:
        return IPMI_CC_INVALID_CMD;
    }
}

/**
 * @brief  IPMI 命令总分发入口。
 * @param  req 已解析且校验通过的请求结构。
 * @param  rsp 响应结构输出指针。
 * @retval 无，执行结果写入 rsp->completion_code。
 */
void ipmi_dispatch(const ipmi_request_t *req, ipmi_response_t *rsp)
{
    if ((req == 0) || (rsp == 0)) {
        return;
    }

    /* 先填写所有响应都需要的公共字段和默认错误码。 */
    rsp->netfn = (uint8_t)(req->netfn + 1u);
    rsp->cmd = req->cmd;
    rsp->data_len = 0u;
    rsp->completion_code = IPMI_CC_INVALID_CMD;

    switch (req->netfn) {
    case IPMI_NETFN_APP:
        rsp->completion_code = ipmi_dispatch_application(req, rsp);
        break;

    case IPMI_NETFN_SENSOR_EVENT:
        rsp->completion_code = ipmi_dispatch_sensor_event(req, rsp);
        break;

    case IPMI_NETFN_STORAGE:
        rsp->completion_code = ipmi_dispatch_storage(req, rsp);
        break;

    default:
        rsp->completion_code = IPMI_CC_INVALID_CMD;
        break;
    }

    /* 非成功响应记录一条警告，便于定位 NetFn、Cmd 或长度错误。 */
    if (rsp->completion_code != IPMI_CC_OK) {
        APP_LOGW("IPMI command failed: netfn=0x%02X cmd=0x%02X cc=0x%02X len=%u",
                 (unsigned int)req->netfn,
                 (unsigned int)req->cmd,
                 (unsigned int)rsp->completion_code,
                 (unsigned int)req->data_len);
    }
}
