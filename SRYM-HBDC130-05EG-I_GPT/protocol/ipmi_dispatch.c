#include "ipmi_dispatch.h"
#include "sensor_manager.h"
#include "sdr_manager.h"
#include "fru_manager.h"
#include "fault_manager.h"
#include <string.h>

static uint8_t g_own_addr_8bit;
static uint8_t g_ga_id;

static uint8_t put_u16_le(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)(v & 0xFFu);
    p[1] = (uint8_t)(v >> 8);
    return 2u;
}

void ipmi_init(uint8_t own_addr_8bit, uint8_t ga_id)
{
    g_own_addr_8bit = own_addr_8bit;
    g_ga_id = ga_id;
    sensor_manager_init();
    fru_manager_init();
    sdr_manager_init(g_own_addr_8bit, (uint8_t)(g_ga_id + 1u));
}

static uint8_t cmd_get_device_id(ipmi_response_t *rsp)
{
    uint32_t manufacturer_id = 0u;
    uint16_t product_id = 0x1305u;

    rsp->data[0] = 0x01u;
    rsp->data[1] = 0x81u;
    rsp->data[2] = APP_FW_MAJOR_BCD;
    rsp->data[3] = APP_FW_MINOR_BCD;
    rsp->data[4] = 0x02u;
    rsp->data[5] = 0x29u;
    rsp->data[6] = (uint8_t)(manufacturer_id & 0xFFu);
    rsp->data[7] = (uint8_t)((manufacturer_id >> 8) & 0xFFu);
    rsp->data[8] = (uint8_t)((manufacturer_id >> 16) & 0xFFu);
    rsp->data[9] = (uint8_t)(product_id & 0xFFu);
    rsp->data[10] = (uint8_t)(product_id >> 8);
    rsp->data[11] = 0u;
    rsp->data[12] = 0u;
    rsp->data[13] = 0u;
    rsp->data[14] = 0u;
    rsp->data_len = 15u;
    return IPMI_CC_OK;
}

static uint8_t cmd_get_self_test(ipmi_response_t *rsp)
{
    if (fault_manager_has_active_fault()) {
        rsp->data[0] = 0x57u; /* Self-test/device-specific error present. */
        rsp->data[1] = (uint8_t)(fault_manager_get_bits() & 0xFFu);
    } else {
        rsp->data[0] = 0x55u;
        rsp->data[1] = 0x00u;
    }
    rsp->data_len = 2u;
    return IPMI_CC_OK;
}

static uint8_t cmd_get_sensor_reading(const ipmi_request_t *req, ipmi_response_t *rsp)
{
    const ipmi_sensor_t *s;

    if (req->data_len != 1u) return IPMI_CC_REQ_LEN_INVALID;
    s = sensor_get(req->data[0]);
    if (s == 0) return IPMI_CC_NOT_PRESENT;

    rsp->data[0] = s->raw_value;
    rsp->data[1] = s->sensor_status;
    rsp->data[2] = (uint8_t)(s->event_status & 0xFFu);
    rsp->data[3] = (uint8_t)(s->event_status >> 8);
    rsp->data_len = 4u;
    return IPMI_CC_OK;
}

static uint8_t cmd_get_sensor_type(const ipmi_request_t *req, ipmi_response_t *rsp)
{
    const ipmi_sensor_t *s;

    if (req->data_len != 1u) return IPMI_CC_REQ_LEN_INVALID;
    s = sensor_get(req->data[0]);
    if (s == 0) return IPMI_CC_NOT_PRESENT;

    rsp->data[0] = s->sensor_type;
    rsp->data[1] = s->reading_type;
    rsp->data_len = 2u;
    return IPMI_CC_OK;
}

static uint8_t cmd_get_sensor_thresholds(const ipmi_request_t *req, ipmi_response_t *rsp)
{
    const ipmi_sensor_t *s;

    if (req->data_len != 1u) return IPMI_CC_REQ_LEN_INVALID;
    s = sensor_get(req->data[0]);
    if (s == 0) return IPMI_CC_NOT_PRESENT;
    if (s->kind == SENSOR_KIND_DISCRETE) return IPMI_CC_INVALID_CMD;

    rsp->data[0] = s->threshold_mask;
    rsp->data[1] = s->lnr;
    rsp->data[2] = s->lc;
    rsp->data[3] = s->lnc;
    rsp->data[4] = s->unc;
    rsp->data[5] = s->uc;
    rsp->data[6] = s->unr;
    rsp->data_len = 7u;
    return IPMI_CC_OK;
}

static uint8_t cmd_get_sdr_info(ipmi_response_t *rsp)
{
    rsp->data[0] = sdr_get_count();
    rsp->data[1] = 0x01u;
    rsp->data_len = 2u;
    return IPMI_CC_OK;
}

static uint8_t cmd_reserve_sdr(ipmi_response_t *rsp)
{
    uint16_t id = sdr_reserve();
    put_u16_le(rsp->data, id);
    rsp->data_len = 2u;
    return IPMI_CC_OK;
}

static uint8_t cmd_get_sdr(const ipmi_request_t *req, ipmi_response_t *rsp)
{
    uint16_t record_id;
    uint8_t count;

    if (req->data_len != 6u) return IPMI_CC_REQ_LEN_INVALID;
    record_id = (uint16_t)req->data[2] | ((uint16_t)req->data[3] << 8);
    count = sdr_read_record(record_id, req->data[4], req->data[5], &rsp->data[0], &rsp->data[1], &rsp->data[2]);
    if (count == 0u) return IPMI_CC_PARAM_OUT_OF_RANGE;
    rsp->data_len = (uint8_t)(count + 2u);
    return IPMI_CC_OK;
}

static uint8_t cmd_get_fru_info(const ipmi_request_t *req, ipmi_response_t *rsp)
{
    uint16_t sz;

    if (req->data_len != 1u) return IPMI_CC_REQ_LEN_INVALID;
    if (req->data[0] != FRU_DEVICE_ID_POWER_MODULE) return IPMI_CC_NOT_PRESENT;

    sz = fru_get_area_size();
    rsp->data[0] = (uint8_t)(sz & 0xFFu);
    rsp->data[1] = (uint8_t)(sz >> 8);
    rsp->data[2] = 0x00u;
    rsp->data_len = 3u;
    return IPMI_CC_OK;
}

static uint8_t cmd_read_fru(const ipmi_request_t *req, ipmi_response_t *rsp)
{
    uint16_t offset;
    uint8_t n;

    if (req->data_len != 4u) return IPMI_CC_REQ_LEN_INVALID;
    if (req->data[0] != FRU_DEVICE_ID_POWER_MODULE) return IPMI_CC_NOT_PRESENT;

    offset = (uint16_t)req->data[1] | ((uint16_t)req->data[2] << 8);
    n = fru_read(offset, req->data[3], &rsp->data[1]);
    if (n == 0u) return IPMI_CC_PARAM_OUT_OF_RANGE;
    rsp->data[0] = n;
    rsp->data_len = (uint8_t)(n + 1u);
    return IPMI_CC_OK;
}

static uint8_t dispatch_app(const ipmi_request_t *req, ipmi_response_t *rsp)
{
    switch (req->cmd) {
    case IPMI_CMD_GET_DEVICE_ID:
        if (req->data_len != 0u) return IPMI_CC_REQ_LEN_INVALID;
        return cmd_get_device_id(rsp);
    case IPMI_CMD_GET_SELF_TEST:
        if (req->data_len != 0u) return IPMI_CC_REQ_LEN_INVALID;
        return cmd_get_self_test(rsp);
    default:
        return IPMI_CC_INVALID_CMD;
    }
}

static uint8_t dispatch_sensor(const ipmi_request_t *req, ipmi_response_t *rsp)
{
    switch (req->cmd) {
    case IPMI_CMD_GET_DEVICE_SDR_INFO:
        if (req->data_len != 0u) return IPMI_CC_REQ_LEN_INVALID;
        return cmd_get_sdr_info(rsp);
    case IPMI_CMD_RESERVE_DEVICE_SDR:
        if (req->data_len != 0u) return IPMI_CC_REQ_LEN_INVALID;
        return cmd_reserve_sdr(rsp);
    case IPMI_CMD_GET_DEVICE_SDR:
        return cmd_get_sdr(req, rsp);
    case IPMI_CMD_GET_SENSOR_THRESH:
        return cmd_get_sensor_thresholds(req, rsp);
    case IPMI_CMD_GET_SENSOR_READING:
        return cmd_get_sensor_reading(req, rsp);
    case IPMI_CMD_GET_SENSOR_TYPE:
        return cmd_get_sensor_type(req, rsp);
    default:
        return IPMI_CC_INVALID_CMD;
    }
}

static uint8_t dispatch_storage(const ipmi_request_t *req, ipmi_response_t *rsp)
{
    switch (req->cmd) {
    case IPMI_CMD_GET_FRU_INFO:
        return cmd_get_fru_info(req, rsp);
    case IPMI_CMD_READ_FRU_DATA:
        return cmd_read_fru(req, rsp);
    case IPMI_CMD_WRITE_FRU_DATA:
        return IPMI_CC_INVALID_CMD;
    default:
        return IPMI_CC_INVALID_CMD;
    }
}

void ipmi_dispatch(const ipmi_request_t *req, ipmi_response_t *rsp)
{
    if ((req == 0) || (rsp == 0)) return;

    rsp->netfn = (uint8_t)(req->netfn + 1u);
    rsp->cmd = req->cmd;
    rsp->data_len = 0u;
    rsp->completion_code = IPMI_CC_INVALID_CMD;

    switch (req->netfn) {
    case IPMI_NETFN_APP:
        rsp->completion_code = dispatch_app(req, rsp);
        break;
    case IPMI_NETFN_SENSOR_EVENT:
        rsp->completion_code = dispatch_sensor(req, rsp);
        break;
    case IPMI_NETFN_STORAGE:
        rsp->completion_code = dispatch_storage(req, rsp);
        break;
    default:
        rsp->completion_code = IPMI_CC_INVALID_CMD;
        break;
    }
}
