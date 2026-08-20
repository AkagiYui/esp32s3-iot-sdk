#include "dns_message.h"

#include <string.h>

#define DNS_HEADER_LEN 12
#define DNS_MAX_LABEL_LEN 63
#define DNS_TYPE_A 1
#define DNS_CLASS_IN 1

static uint16_t read_u16(const uint8_t *buffer)
{
    return (uint16_t)((buffer[0] << 8) | buffer[1]);
}

static void write_u16(uint8_t *buffer, uint16_t value)
{
    buffer[0] = (uint8_t)(value >> 8);
    buffer[1] = (uint8_t)(value & 0xff);
}

static void write_u32(uint8_t *buffer, uint32_t value)
{
    buffer[0] = (uint8_t)(value >> 24);
    buffer[1] = (uint8_t)(value >> 16);
    buffer[2] = (uint8_t)(value >> 8);
    buffer[3] = (uint8_t)(value & 0xff);
}

/** 走完 question 段的 QNAME，返回 QNAME 之后的偏移；0 表示报文非法。 */
static size_t skip_qname(const uint8_t *query, size_t query_len, size_t offset)
{
    while (offset < query_len) {
        uint8_t length = query[offset];

        if (length == 0) {
            return offset + 1;
        }

        /* question 段里不允许出现压缩指针。 */
        if ((length & 0xc0) != 0 || length > DNS_MAX_LABEL_LEN) {
            return 0;
        }

        offset += (size_t)length + 1;
    }

    return 0;
}

size_t dns_message_build_response(const uint8_t *query, size_t query_len, uint32_t answer_ip_be,
                                  uint32_t ttl_seconds, uint8_t *out, size_t out_size)
{
    if (query == NULL || out == NULL || query_len < DNS_HEADER_LEN) {
        return 0;
    }

    /* 只处理标准查询：QR=0、OPCODE=0。 */
    if ((query[2] & 0x80) != 0 || ((query[2] >> 3) & 0x0f) != 0) {
        return 0;
    }

    if (read_u16(&query[4]) != 1) {
        return 0;
    }

    size_t qname_end = skip_qname(query, query_len, DNS_HEADER_LEN);
    if (qname_end == 0 || qname_end + 4 > query_len) {
        return 0;
    }

    uint16_t qtype = read_u16(&query[qname_end]);
    uint16_t qclass = read_u16(&query[qname_end + 2]);
    size_t question_len = qname_end + 4 - DNS_HEADER_LEN;
    bool answerable = (qtype == DNS_TYPE_A && qclass == DNS_CLASS_IN);

    size_t response_len = DNS_HEADER_LEN + question_len + (answerable ? DNS_MESSAGE_ANSWER_LEN : 0);
    if (response_len > out_size) {
        return 0;
    }

    memcpy(out, query, DNS_HEADER_LEN + question_len);

    uint8_t recursion_desired = query[2] & 0x01;
    out[2] = (uint8_t)(0x84 | recursion_desired);        /* QR=1, AA=1, RD 原样回带 */
    out[3] = (uint8_t)(recursion_desired ? 0x80 : 0x00); /* RA，RCODE=NOERROR */
    write_u16(&out[4], 1);
    write_u16(&out[6], answerable ? 1 : 0);
    write_u16(&out[8], 0);
    write_u16(&out[10], 0);

    if (!answerable) {
        return response_len;
    }

    uint8_t *answer = &out[DNS_HEADER_LEN + question_len];
    write_u16(&answer[0], 0xc00c); /* 指向 question 段的 QNAME */
    write_u16(&answer[2], DNS_TYPE_A);
    write_u16(&answer[4], DNS_CLASS_IN);
    write_u32(&answer[6], ttl_seconds);
    write_u16(&answer[10], 4);
    memcpy(&answer[12], &answer_ip_be, sizeof(answer_ip_be));

    return response_len;
}
