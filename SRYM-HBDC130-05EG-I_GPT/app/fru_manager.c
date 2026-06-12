#include "fru_manager.h"
#include <string.h>

static uint8_t g_fru[FRU_AREA_MAX_SIZE];
static uint16_t g_fru_size;

static uint8_t checksum8(const uint8_t *p, uint16_t n)
{
    uint8_t s = 0u;
    uint16_t i;
    for (i = 0u; i < n; i++) s = (uint8_t)(s + p[i]);
    return (uint8_t)(0u - s);
}

static uint8_t verify_sum0(const uint8_t *p, uint16_t n)
{
    uint8_t s = 0u;
    uint16_t i;
    for (i = 0u; i < n; i++) s = (uint8_t)(s + p[i]);
    return s;
}

static uint16_t append_field(uint16_t pos, const char *s)
{
    uint8_t len = (uint8_t)strlen(s);
    if (len > 63u) len = 63u;
    g_fru[pos++] = (uint8_t)(0xC0u | len);
    memcpy(&g_fru[pos], s, len);
    return (uint16_t)(pos + len);
}

static uint16_t align_area(uint16_t start, uint16_t pos)
{
    uint16_t len_with_checksum;
    uint16_t pad;

    g_fru[pos++] = 0xC1u;
    len_with_checksum = (uint16_t)(pos - start + 1u);
    pad = (uint16_t)((8u - (len_with_checksum & 7u)) & 7u);
    while (pad-- > 0u) g_fru[pos++] = 0u;

    g_fru[start + 1u] = (uint8_t)((pos - start + 1u) / 8u);
    g_fru[pos] = checksum8(&g_fru[start], (uint16_t)(pos - start));
    return (uint16_t)(pos + 1u);
}

static uint16_t build_board_area(uint16_t start)
{
    uint16_t pos = start;

    g_fru[pos++] = 0x01u;  /* Format version. */
    g_fru[pos++] = 0x00u;  /* Area length placeholder. */
    g_fru[pos++] = 0x00u;  /* Language code: English. */
    g_fru[pos++] = 0x00u;  /* Mfg time, minutes since 1996-01-01, LSB. */
    g_fru[pos++] = 0x00u;
    g_fru[pos++] = 0x00u;
    pos = append_field(pos, "TBD");
    pos = append_field(pos, "HBDC130-05EG-I Power Module");
    pos = append_field(pos, "0000000000");
    pos = append_field(pos, "HBDC130-05EG-I");
    pos = append_field(pos, "HBDC130-IPMI-V2");
    return align_area(start, pos);
}

static uint16_t build_product_area(uint16_t start)
{
    uint16_t pos = start;

    g_fru[pos++] = 0x01u;  /* Format version. */
    g_fru[pos++] = 0x00u;  /* Area length placeholder. */
    g_fru[pos++] = 0x00u;  /* Language code: English. */
    pos = append_field(pos, "TBD");
    pos = append_field(pos, "HBDC130-05EG-I Power Module");
    pos = append_field(pos, "HBDC130-05EG-I");
    pos = append_field(pos, "HW:A FW:2.0.0");
    pos = append_field(pos, "0000000000");
    pos = append_field(pos, "");
    pos = append_field(pos, "HBDC130-IPMI-V2");
    return align_area(start, pos);
}

void fru_manager_init(void)
{
    uint16_t board_start;
    uint16_t product_start;
    uint16_t pos;

    memset(g_fru, 0, sizeof(g_fru));

    board_start = 8u;
    pos = build_board_area(board_start);

    product_start = pos;
    pos = build_product_area(product_start);

    g_fru[0] = 0x01u;
    g_fru[1] = 0x00u;
    g_fru[2] = 0x00u;
    g_fru[3] = (uint8_t)(board_start / 8u);
    g_fru[4] = (uint8_t)(product_start / 8u);
    g_fru[5] = 0x00u;
    g_fru[6] = 0x00u;
    g_fru[7] = checksum8(g_fru, 7u);

    g_fru_size = pos;
}

uint16_t fru_get_area_size(void)
{
    return g_fru_size;
}

uint8_t fru_read(uint16_t offset, uint8_t count, uint8_t *out)
{
    uint8_t n;

    if ((out == 0) || (count == 0u) || (offset >= g_fru_size)) return 0u;
    if (verify_sum0(g_fru, 8u) != 0u) return 0u;

    n = count;
    if (n > IPMB_MAX_READ_CHUNK) n = IPMB_MAX_READ_CHUNK;
    if ((uint16_t)(offset + n) > g_fru_size) n = (uint8_t)(g_fru_size - offset);
    memcpy(out, &g_fru[offset], n);
    return n;
}
