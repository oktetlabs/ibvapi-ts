/* SPDX-License-Identifier: Apache-2.0
 * Copyright (C) 2012-2022 OKTET Labs Ltd.
 */
/** @file
 * @brief InfiniBand Verbs API Test Suite
 *
 * Common includes and definitions.
 *
 * @author Yurij Plotnikov <Yurij.Plotnikov@oktetlabs.ru>
 */

#ifndef __TS_IBVAPI_TS_H__
#define __TS_IBVAPI_TS_H__

#include <netinet/ether.h>
#include <netinet/udp.h>
#include <netinet/ip.h>

#include "te_config.h"

#include "tapi_rpc.h"
#include "tapi_env.h"
#include "tapi_rpc_verbs.h"

/* Reasonable TTL */
#define IBVTS_TTL 5

#ifdef __cplusplus
extern "C" {
#endif

/** All layers headers for udp over ip over ethernet packet */
typedef struct te_eth_ip_udp_hdr {
    struct ether_header ethhdr;
    struct iphdr        iphdr;
    struct udphdr       udphdr;
} __attribute__ ((packed)) te_eth_ip_udp_hdr;

/**
 * Create raw packet with ethernet, ip and udp header.
 *
 * @param src_laddr      Source link layer address
 * @param dst_laddr      Destination link layer address
 * @param src_addr       Source ip layer address
 * @param dst_addr       Source ip layer address
 * @param seq_num        Packet sequence number
 * @param multicast      Create multicast packet or UDP packet
 * @param buf            Payload buffer
 * @param payload_length Length of data in buf
 * @param pkt            Pointer to the buffer to save packet (OUT)
 *
 * @return  Length of created raw packet
 */
extern int ibvts_create_raw_udp_dgm(const struct sockaddr *src_laddr,
                                    const struct sockaddr *dst_laddr,
                                    const struct sockaddr *src_addr,
                                    const struct sockaddr *dst_addr,
                                    uint16_t seq_num, te_bool multicast,
                                    char *buf, uint16_t payload_len,
                                    uint8_t *pkt);

/**
 * Create multicast group ID from multicast address
 *
 * @param addr Multicast address
 * @param gid  Multicast group ID (OUT)
 */
extern void ibvts_fill_gid(const struct sockaddr *addr,
                           union rpc_ibv_gid *gid);

#ifdef __cplusplus
} /* extern "C" */

#endif
#endif /* !__TS_IBVAPI_TS_H__ */
