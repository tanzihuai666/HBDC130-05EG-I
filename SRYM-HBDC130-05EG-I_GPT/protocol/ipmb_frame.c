/**
 * @file    ipmb_frame.c
 * @brief   IPMB 帧校验、Request 解析和 Response 构造实现。
 *          本模块只处理纯协议字节，不直接操作 I2C 外设。这样可以将底层总线状态机与
 *          IPMI 命令处理解耦，并便于后续使用主机测试程序验证校验和及字段打包逻辑。
 * @author  GPT
 * @date    2026-06-15
 * @version V0.4
 */
#include "ipmb_frame.h"

/**
 * @brief  计算 IPMB 8 位补码校验和。
 * @param  buf 待计算数据指针。
 * @param  len 待计算字节数。
 * @retval 使“原数据所有字节之和 + 返回值”低 8 位等于 0 的校验字节。
 */
uint8_t ipmb_checksum(const uint8_t *buf, uint8_t len)
{
    uint8_t sum;
    uint8_t index;

    if (buf == 0) {
        return 0u;
    }

    sum = 0u;

    for (index = 0u; index < len; index++) {
        sum = (uint8_t)(sum + buf[index]);
    }

    return (uint8_t)(0u - sum);
}

/**
 * @brief  校验一段包含校验字节的 IPMB 数据。
 * @param  buf 数据指针，范围内必须包含校验字节。
 * @param  len 总字节数。
 * @retval true=所有字节累加和低 8 位为 0；false=校验失败或参数为空。
 */
bool ipmb_verify_checksum(const uint8_t *buf, uint8_t len)
{
    uint8_t sum;
    uint8_t index;

    if (buf == 0) {
        return false;
    }

    sum = 0u;

    for (index = 0u; index < len; index++) {
        sum = (uint8_t)(sum + buf[index]);
    }

    return (sum == 0u) ? true : false;
}

/**
 * @brief  解析一个标准 IPMB Request 帧。
 * @param  frame          完整 Request 帧指针。
 * @param  len            帧总长度，包含两个校验字节。
 * @param  own_addr_8bit  本机 8 位 IPMB 地址，用于过滤非本机请求。
 * @param  req            解析结果输出结构体。
 * @retval true=帧格式和两个校验和均正确；false=参数、地址、长度或校验错误。
 *
 * @note   Request 帧格式：
 *         [0] RsSA
 *         [1] NetFn/LUN
 *         [2] Checksum1
 *         [3] RqSA
 *         [4] Seq/LUN
 *         [5] Cmd
 *         [6..N-2] Data
 *         [N-1] Checksum2
 */
bool ipmb_parse_request(const uint8_t *frame,
                        uint8_t len,
                        uint8_t own_addr_8bit,
                        ipmi_request_t *req)
{
    if ((frame == 0) || (req == 0)) {
        return false;
    }

    /* 最短 Request 由 6 个固定字段和第二校验字节组成，共 7 字节。 */
    if (len < 7u) {
        return false;
    }

    /* I2C 驱动上报的首字节必须是本机 8 位响应地址。 */
    if (frame[0] != own_addr_8bit) {
        return false;
    }

    /* 第一校验域覆盖 RsSA 和 NetFn/LUN 以及 Checksum1，共 3 字节。 */
    if (!ipmb_verify_checksum(&frame[0], 3u)) {
        return false;
    }

    /* 第二校验域从 RqSA 开始，覆盖至帧尾 Checksum2。 */
    if (!ipmb_verify_checksum(&frame[3], (uint8_t)(len - 3u))) {
        return false;
    }

    /* 将打包字段拆分后保存到统一 Request 结构体。 */
    req->rs_sa = frame[0];
    req->netfn = IPMB_GET_NETFN(frame[1]);
    req->rs_lun = IPMB_GET_LUN(frame[1]);
    req->rq_sa = frame[3];
    req->seq = IPMB_GET_SEQ(frame[4]);
    req->rq_lun = IPMB_GET_LUN(frame[4]);
    req->cmd = frame[5];

    /* data 指向原始接收缓冲区内部，调用方必须在缓冲区有效期内完成命令处理。 */
    req->data = &frame[6];

    /* 总长度减去 6 个固定字段和帧尾校验字节，得到数据区长度。 */
    req->data_len = (uint8_t)(len - 7u);

    return true;
}

/**
 * @brief  根据 Request 和 IPMI 处理结果构造标准 IPMB Response 帧。
 * @param  req            原始请求解析结果，用于回填请求方地址、序号、LUN 和 Cmd。
 * @param  rsp            IPMI 响应内容，包含 Completion Code 和响应数据。
 * @param  own_addr_8bit  本机 8 位 IPMB 地址。
 * @param  out            完整响应帧输出缓冲区，容量至少为 IPMB_MAX_FRAME_LEN。
 * @retval 构造完成的响应帧长度；参数错误时返回 0。
 *
 * @note   Response 帧格式：
 *         [0] RqSA
 *         [1] Response NetFn/LUN
 *         [2] Checksum1
 *         [3] RsSA
 *         [4] Seq/LUN
 *         [5] Cmd
 *         [6] Completion Code
 *         [7..N-2] Response Data
 *         [N-1] Checksum2
 */
uint8_t ipmb_build_response(const ipmi_request_t *req,
                            const ipmi_response_t *rsp,
                            uint8_t own_addr_8bit,
                            uint8_t *out)
{
    uint8_t data_index;
    uint8_t frame_index;

    if ((req == 0) || (rsp == 0) || (out == 0)) {
        return 0u;
    }

    /* 第一校验域：目标地址、响应 NetFn/LUN、Checksum1。 */
    out[0] = req->rq_sa;
    out[1] = IPMB_MAKE_NETFN_LUN((uint8_t)(req->netfn + 1u), req->rq_lun);
    out[2] = ipmb_checksum(out, 2u);

    /* 第二校验域固定字段。 */
    out[3] = own_addr_8bit;
    out[4] = IPMB_MAKE_SEQ_LUN(req->seq, req->rs_lun);
    out[5] = req->cmd;
    out[6] = rsp->completion_code;

    frame_index = 7u;

    /*
     * 拷贝响应数据时保留最后 1 字节给 Checksum2。
     * 即使上层长度配置错误，也不会写出 IPMB_MAX_FRAME_LEN 缓冲区。
     */
    for (data_index = 0u;
         (data_index < rsp->data_len) &&
         (frame_index < (IPMB_MAX_FRAME_LEN - 1u));
         data_index++) {
        out[frame_index] = rsp->data[data_index];
        frame_index++;
    }

    /* 第二校验和从本机地址 out[3] 开始计算，不包含第一校验域。 */
    out[frame_index] = ipmb_checksum(&out[3], (uint8_t)(frame_index - 3u));
    frame_index++;

    return frame_index;
}
