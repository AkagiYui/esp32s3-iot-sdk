#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/** 构造应答所需的最大额外字节数（answer RR 固定 16 字节）。 */
#define DNS_MESSAGE_ANSWER_LEN 16

/**
 * 为 captive portal 构造 DNS 应答。
 *
 * 行为遵循 RFC 1035 的最小可用子集：
 * - 只应答标准查询（QR=0、OPCODE=0、QDCOUNT=1）
 * - `A`/`IN` 查询返回 `answer_ip_be` 指向的地址
 * - 其它类型（例如 `AAAA`）返回 NOERROR + 0 条记录，客户端会自行回退到 `A`
 * - 不回显 additional 段，避免把客户端的 EDNS OPT 原样打回去
 *
 * @param query        收到的原始报文
 * @param query_len    报文长度
 * @param answer_ip_be 网络字节序的 IPv4 地址
 * @param ttl_seconds  应答 TTL
 * @param out          输出缓冲区
 * @param out_size     输出缓冲区容量
 * @return 应答长度；0 表示不应答。
 */
size_t dns_message_build_response(const uint8_t *query, size_t query_len, uint32_t answer_ip_be,
                                  uint32_t ttl_seconds, uint8_t *out, size_t out_size);
