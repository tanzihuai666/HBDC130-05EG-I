#include "ipmb_driver.h"
#include "ipmb_frame.h"
#include "ipmi_dispatch.h"
#include "bsp_i2c.h"
#include <string.h>

#define IPMB_RX_PENDING_MAX     1u

typedef struct {
    uint8_t data[IPMB_MAX_FRAME_LEN];
    uint8_t len;
    uint8_t dest_addr_8bit;
    uint8_t retry;
    uint8_t used;
} ipmb_tx_item_t;

static uint8_t g_own_addr_8bit;
static volatile uint8_t g_rx_pending;
static uint8_t g_rx_buf[IPMB_MAX_FRAME_LEN];
static uint8_t g_rx_len;
static ipmb_tx_item_t g_txq[IPMB_TX_QUEUE_DEPTH];
static uint8_t g_tx_head;
static uint8_t g_tx_tail;
static uint32_t g_rx_count;
static uint32_t g_tx_count;
static uint32_t g_drop_count;

static void ipmb_rx_callback(const uint8_t *data, uint8_t len)
{
    if ((data == 0) || (len == 0u) || (len > IPMB_MAX_FRAME_LEN)) {
        g_drop_count++;
        return;
    }

    if (g_rx_pending >= IPMB_RX_PENDING_MAX) {
        g_drop_count++;
        return;
    }

    memcpy(g_rx_buf, data, len);
    g_rx_len = len;
    g_rx_pending = 1u;
    g_rx_count++;
}

static uint8_t tx_next(uint8_t idx)
{
    idx++;
    if (idx >= IPMB_TX_QUEUE_DEPTH) idx = 0u;
    return idx;
}

static int tx_is_full(void)
{
    return (tx_next(g_tx_tail) == g_tx_head) ? 1 : 0;
}

static int tx_is_empty(void)
{
    return (g_tx_head == g_tx_tail) ? 1 : 0;
}

static void tx_push(uint8_t dest_addr_8bit, const uint8_t *data, uint8_t len)
{
    ipmb_tx_item_t *item;

    if ((data == 0) || (len == 0u) || (len > IPMB_MAX_FRAME_LEN) || tx_is_full()) {
        g_drop_count++;
        return;
    }

    item = &g_txq[g_tx_tail];
    memcpy(item->data, data, len);
    item->len = len;
    item->dest_addr_8bit = dest_addr_8bit;
    item->retry = 0u;
    item->used = 1u;
    g_tx_tail = tx_next(g_tx_tail);
}

static void tx_task(void)
{
    ipmb_tx_item_t *item;

    if (tx_is_empty()) return;

    item = &g_txq[g_tx_head];
    if (item->used == 0u) {
        g_tx_head = tx_next(g_tx_head);
        return;
    }

    if (bsp_i2c1_master_write((uint8_t)(item->dest_addr_8bit >> 1), item->data, item->len)) {
        item->used = 0u;
        g_tx_head = tx_next(g_tx_head);
        g_tx_count++;
    } else {
        item->retry++;
        if (item->retry >= IPMB_TX_RETRY_MAX) {
            item->used = 0u;
            g_tx_head = tx_next(g_tx_head);
            g_drop_count++;
        }
    }
}

static void rx_task(void)
{
    uint8_t local[IPMB_MAX_FRAME_LEN];
    uint8_t len;
    ipmi_request_t req;
    ipmi_response_t rsp;
    uint8_t out[IPMB_MAX_FRAME_LEN];
    uint8_t out_len;

    if (g_rx_pending == 0u) return;

    len = g_rx_len;
    memcpy(local, g_rx_buf, len);
    g_rx_pending = 0u;

    if (!ipmb_parse_request(local, len, g_own_addr_8bit, &req)) {
        g_drop_count++;
        return;
    }

    ipmi_dispatch(&req, &rsp);
    out_len = ipmb_build_response(&req, &rsp, g_own_addr_8bit, out);
    if (out_len == 0u) {
        g_drop_count++;
        return;
    }

    tx_push(req.rq_sa, out, out_len);
}

void ipmb_driver_init(uint8_t own_addr_8bit)
{
    g_own_addr_8bit = own_addr_8bit;
    g_rx_pending = 0u;
    g_rx_len = 0u;
    g_tx_head = 0u;
    g_tx_tail = 0u;
    g_rx_count = 0u;
    g_tx_count = 0u;
    g_drop_count = 0u;
    memset(g_txq, 0, sizeof(g_txq));
    bsp_i2c1_register_rx_callback(ipmb_rx_callback);
}

void ipmb_driver_task(void)
{
    rx_task();
    tx_task();
}

uint32_t ipmb_driver_get_rx_count(void)
{
    return g_rx_count;
}

uint32_t ipmb_driver_get_tx_count(void)
{
    return g_tx_count;
}

uint32_t ipmb_driver_get_drop_count(void)
{
    return g_drop_count;
}
