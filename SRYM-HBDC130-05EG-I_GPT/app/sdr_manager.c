#include "sdr_manager.h"
#include "sensor_manager.h"
#include <string.h>

static uint8_t g_sdr[SDR_RECORD_COUNT][SDR_MAX_RECORD_SIZE];
static uint8_t g_sdr_len[SDR_RECORD_COUNT];
static uint16_t g_reservation = 1u;

static uint8_t sdr_encode_exp(int8_t r_exp, int8_t b_exp)
{
    return (uint8_t)(((b_exp & 0x0F) << 4) | (r_exp & 0x0F));
}

static uint8_t put_name(uint8_t *r, uint8_t pos, const char *name)
{
    uint8_t len = (uint8_t)strlen(name);
    if (len > 15u) len = 15u;
    r[pos++] = (uint8_t)(0xC0u | len);
    memcpy(&r[pos], name, len);
    return (uint8_t)(pos + len);
}

static void make_record(uint8_t idx, uint8_t owner, uint8_t entity_instance)
{
    const ipmi_sensor_t *s;
    uint8_t *r;
    uint8_t pos;
    uint16_t record_id;

    s = sensor_get((uint8_t)(idx + 1u));
    if (s == 0) return;

    r = g_sdr[idx];
    memset(r, 0, SDR_MAX_RECORD_SIZE);
    record_id = (uint16_t)(idx + 1u);

    r[0] = (uint8_t)(record_id & 0xFFu);
    r[1] = (uint8_t)(record_id >> 8);
    r[2] = 0x51u;
    r[3] = 0x01u;
    r[4] = 0u;
    r[5] = owner;
    r[6] = 0x00u;
    r[7] = s->sensor_id;
    r[8] = s->entity_id;
    r[9] = entity_instance;
    r[10] = 0x67u;
    r[11] = (s->kind == SENSOR_KIND_DISCRETE) ? 0x40u : 0x68u;
    r[12] = s->sensor_type;
    r[13] = s->reading_type;
    r[14] = (s->kind == SENSOR_KIND_DISCRETE) ? 0x3Fu : 0x3Fu;
    r[15] = 0x00u;
    r[16] = 0x00u;
    r[17] = 0x00u;
    r[18] = 0x00u;
    r[19] = 0x00u;
    r[20] = (s->kind == SENSOR_KIND_ANALOG_S8) ? 0x80u : 0x00u;
    r[21] = s->base_unit;
    r[22] = 0x00u;
    r[23] = 0x00u;
    r[24] = (uint8_t)(s->m & 0xFF);
    r[25] = (uint8_t)((s->m >> 8) & 0x03);
    r[26] = (uint8_t)(s->b & 0xFF);
    r[27] = (uint8_t)((s->b >> 8) & 0x03);
    r[28] = 0x00u;
    r[29] = sdr_encode_exp(s->r_exp, s->b_exp);
    r[30] = 0x00u;
    r[31] = s->raw_value;
    r[32] = 0xFFu;
    r[33] = 0x00u;
    r[34] = 0xFFu;
    r[35] = 0x00u;
    r[36] = s->unr;
    r[37] = s->uc;
    r[38] = s->unc;
    r[39] = s->lnr;
    r[40] = s->lc;
    r[41] = s->lnc;
    r[42] = 0x01u;
    r[43] = 0x01u;
    r[44] = 0x00u;
    r[45] = 0x00u;
    r[46] = 0x00u;
    pos = put_name(r, 47u, s->name);
    r[4] = (uint8_t)(pos - 5u);
    g_sdr_len[idx] = pos;
}

void sdr_manager_init(uint8_t owner_addr_8bit, uint8_t entity_instance)
{
    uint8_t i;
    for (i = 0u; i < SDR_RECORD_COUNT; i++) {
        make_record(i, owner_addr_8bit, entity_instance);
    }
}

uint8_t sdr_get_count(void)
{
    return SDR_RECORD_COUNT;
}

uint16_t sdr_reserve(void)
{
    g_reservation++;
    if (g_reservation == 0u) g_reservation = 1u;
    return g_reservation;
}

uint8_t sdr_read_record(uint16_t record_id, uint8_t offset, uint8_t count, uint8_t *next_lsb, uint8_t *next_msb, uint8_t *out)
{
    uint8_t idx;
    uint8_t n;

    if ((next_lsb == 0) || (next_msb == 0) || (out == 0)) return 0u;
    if (record_id == 0u) record_id = 1u;
    if ((record_id < 1u) || (record_id > SDR_RECORD_COUNT)) return 0u;

    idx = (uint8_t)(record_id - 1u);
    if (offset >= g_sdr_len[idx]) return 0u;

    if (record_id >= SDR_RECORD_COUNT) {
        *next_lsb = 0xFFu;
        *next_msb = 0xFFu;
    } else {
        *next_lsb = (uint8_t)((record_id + 1u) & 0xFFu);
        *next_msb = 0x00u;
    }

    n = count;
    if (n == 0xFFu) n = (uint8_t)(g_sdr_len[idx] - offset);
    if (n > IPMB_MAX_READ_CHUNK) n = IPMB_MAX_READ_CHUNK;
    if ((uint8_t)(offset + n) > g_sdr_len[idx]) n = (uint8_t)(g_sdr_len[idx] - offset);
    memcpy(out, &g_sdr[idx][offset], n);
    return n;
}
