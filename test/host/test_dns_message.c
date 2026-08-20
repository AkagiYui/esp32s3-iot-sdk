#include "dns_message.h"
#include "test_support.h"

/** 按内存顺序拼出 IPv4 地址，避免测试依赖主机字节序。 */
static uint32_t ip_bytes(uint8_t a, uint8_t b, uint8_t c, uint8_t d)
{
    uint32_t value = 0;
    const uint8_t bytes[4] = {a, b, c, d};
    memcpy(&value, bytes, sizeof(bytes));
    return value;
}

#define AP_IP ip_bytes(192, 168, 6, 1)

static size_t build_query(uint8_t *out, uint16_t qtype, uint16_t qclass)
{
    static const uint8_t header[] = {
        0x12, 0x34, /* id */
        0x01, 0x00, /* QR=0, RD=1 */
        0x00, 0x01, /* qdcount */
        0x00, 0x00, /* ancount */
        0x00, 0x00, /* nscount */
        0x00, 0x00, /* arcount */
    };
    static const uint8_t qname[] = {
        3, 'w', 'w', 'w', 7, 'e', 'x', 'a', 'm', 'p', 'l', 'e', 3, 'c', 'o', 'm', 0,
    };

    size_t offset = 0;
    memcpy(out + offset, header, sizeof(header));
    offset += sizeof(header);
    memcpy(out + offset, qname, sizeof(qname));
    offset += sizeof(qname);
    out[offset++] = (uint8_t)(qtype >> 8);
    out[offset++] = (uint8_t)(qtype & 0xff);
    out[offset++] = (uint8_t)(qclass >> 8);
    out[offset++] = (uint8_t)(qclass & 0xff);
    return offset;
}

static void test_a_query_gets_answer(void)
{
    uint8_t query[64];
    uint8_t response[128];
    size_t query_len = build_query(query, 1, 1);

    size_t len = dns_message_build_response(query, query_len, AP_IP, 60, response, sizeof(response));
    CHECK_INT(len, query_len + DNS_MESSAGE_ANSWER_LEN);

    CHECK_INT(response[0], 0x12);
    CHECK_INT(response[1], 0x34);
    CHECK_INT(response[2], 0x85); /* QR=1, AA=1, RD=1 */
    CHECK_INT(response[3], 0x80); /* RA=1, RCODE=0 */
    CHECK_INT(response[4] << 8 | response[5], 1);
    CHECK_INT(response[6] << 8 | response[7], 1);
    CHECK_INT(response[8] << 8 | response[9], 0);
    CHECK_INT(response[10] << 8 | response[11], 0);

    const uint8_t *answer = response + query_len;
    CHECK_INT(answer[0], 0xc0);
    CHECK_INT(answer[1], 0x0c);
    CHECK_INT(answer[2] << 8 | answer[3], 1); /* type A */
    CHECK_INT(answer[4] << 8 | answer[5], 1); /* class IN */
    CHECK_INT(answer[9], 60);                 /* TTL 低字节 */
    CHECK_INT(answer[10] << 8 | answer[11], 4);
    CHECK_INT(answer[12], 192);
    CHECK_INT(answer[13], 168);
    CHECK_INT(answer[14], 6);
    CHECK_INT(answer[15], 1);
}

static void test_aaaa_query_gets_empty_noerror(void)
{
    uint8_t query[64];
    uint8_t response[128];
    size_t query_len = build_query(query, 28, 1);

    size_t len = dns_message_build_response(query, query_len, AP_IP, 60, response, sizeof(response));
    CHECK_INT(len, query_len);
    CHECK_INT(response[6] << 8 | response[7], 0);
    CHECK_INT(response[3] & 0x0f, 0); /* NOERROR，客户端才会回退到 A */
}

static void test_non_in_class_is_not_answered_with_a(void)
{
    uint8_t query[64];
    uint8_t response[128];
    size_t query_len = build_query(query, 1, 3);

    size_t len = dns_message_build_response(query, query_len, AP_IP, 60, response, sizeof(response));
    CHECK_INT(len, query_len);
    CHECK_INT(response[6] << 8 | response[7], 0);
}

static void test_rejects_malformed_messages(void)
{
    uint8_t query[64];
    uint8_t response[128];
    size_t query_len = build_query(query, 1, 1);

    /* 报文过短 */
    CHECK_INT(dns_message_build_response(query, 8, AP_IP, 60, response, sizeof(response)), 0);

    /* 已经是应答（QR=1） */
    query[2] = 0x81;
    CHECK_INT(dns_message_build_response(query, query_len, AP_IP, 60, response, sizeof(response)), 0);
    query[2] = 0x01;

    /* 非标准查询 opcode */
    query[2] = 0x09;
    CHECK_INT(dns_message_build_response(query, query_len, AP_IP, 60, response, sizeof(response)), 0);
    query[2] = 0x01;

    /* qdcount != 1 */
    query[5] = 2;
    CHECK_INT(dns_message_build_response(query, query_len, AP_IP, 60, response, sizeof(response)), 0);
    query[5] = 1;

    /* QNAME 里出现压缩指针 */
    query[12] = 0xc0;
    CHECK_INT(dns_message_build_response(query, query_len, AP_IP, 60, response, sizeof(response)), 0);
    query[12] = 3;

    /* 标签长度越界 */
    CHECK_INT(dns_message_build_response(query, 14, AP_IP, 60, response, sizeof(response)), 0);

    /* 输出缓冲区不足 */
    CHECK_INT(dns_message_build_response(query, query_len, AP_IP, 60, response, 16), 0);
}

static void test_additional_section_is_dropped(void)
{
    uint8_t query[96];
    uint8_t response[160];
    size_t query_len = build_query(query, 1, 1);

    /* 追加一个 EDNS OPT 记录，并把 arcount 置 1。 */
    query[11] = 1;
    static const uint8_t opt[] = {0x00, 0x00, 0x29, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    memcpy(query + query_len, opt, sizeof(opt));
    size_t total_len = query_len + sizeof(opt);

    size_t len = dns_message_build_response(query, total_len, AP_IP, 60, response, sizeof(response));
    CHECK_INT(len, query_len + DNS_MESSAGE_ANSWER_LEN);
    CHECK_INT(response[10] << 8 | response[11], 0);
}

int main(void)
{
    RUN(test_a_query_gets_answer);
    RUN(test_aaaa_query_gets_empty_noerror);
    RUN(test_non_in_class_is_not_answered_with_a);
    RUN(test_rejects_malformed_messages);
    RUN(test_additional_section_is_dropped);
    TEST_MAIN_END();
}
