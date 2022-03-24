/* SPDX-License-Identifier: Apache-2.0
 * Copyright (C) 2012-2022 OKTET Labs Ltd.
 */
/** @file
 * @brief InfiniBand Verbs API Test Suite
 *
 * Implementation of common functions.
 *
 * @author Yurij Plitnikov <Yurij.Plotnikov@oktetlabs.ru>
 */

/** User name of InfiniBand Verbs API test suite library */
#define TE_LGR_USER     "Library"

#include "ibvapi-ts.h"

/* See description in ibvapi-ts.h */
int
ibvts_create_raw_udp_dgm(const struct sockaddr *src_laddr,
                         const struct sockaddr *dst_laddr,
                         const struct sockaddr *src_addr,
                         const struct sockaddr *dst_addr,
                         uint16_t seq_num, te_bool multicast,
                         char *buf, uint16_t payload_len, uint8_t *pkt)
{
    te_eth_ip_udp_hdr   *packet = (te_eth_ip_udp_hdr *)pkt;
    struct ether_header *ethhdr = &(packet->ethhdr);
    struct iphdr         iphdr;
    struct udphdr        udphdr;
    union rpc_ibv_gid    gid;

    memset(&gid, 0, sizeof(gid));

    /* Make ethernet header */
    ethhdr->ether_type = htons (ETHERTYPE_IP);
    if (multicast)
    {
        ibvts_fill_gid(dst_addr, &gid);
        memcpy(ethhdr->ether_dhost, &gid.raw[10], ETH_ALEN);
    }
    else
        memcpy (ethhdr->ether_dhost, dst_laddr->sa_data, ETH_ALEN);
    memcpy (ethhdr->ether_shost, src_laddr->sa_data, ETH_ALEN);

    /* Make ip header */
    /* Header which doesn't have options has such length */
    memset(&iphdr, 0, sizeof(iphdr));
    iphdr.ihl      = 0x5;
    iphdr.version  = IPVERSION;
    iphdr.tos      = 0x00;
    iphdr.tot_len  = htons (sizeof(struct iphdr) + sizeof(struct udphdr) +
                             payload_len);
    iphdr.id       = seq_num;
    iphdr.frag_off = 0x0000;
    iphdr.ttl      = IBVTS_TTL;
    iphdr.protocol = IPPROTO_UDP;
    iphdr.saddr    = SIN(src_addr)->sin_addr.s_addr;
    iphdr.daddr    = SIN(dst_addr)->sin_addr.s_addr;
    iphdr.check    = 0;
    memcpy(&(packet->iphdr), &iphdr, sizeof(iphdr));

    /* Make udp header */
    memset(&udphdr, 0, sizeof(udphdr));
    udphdr.source  = 0;
    udphdr.dest    = SIN(dst_addr)->sin_port;
    udphdr.len     = htons (sizeof(struct udphdr) + payload_len);
    udphdr.check   = 0;
    memcpy(&(packet->udphdr), &udphdr, sizeof(udphdr));

    /* Copy payload */
    memcpy(&pkt[sizeof(te_eth_ip_udp_hdr)], buf, payload_len);

    /* Return whole packet length */
    return (sizeof(te_eth_ip_udp_hdr) + payload_len);
}

/* See description in ibvapi-ts.h */
void
ibvts_fill_gid(const struct sockaddr *addr, union rpc_ibv_gid *gid)
{
    unsigned char   mmac[ETH_ALEN];

    memset(gid, 0, sizeof(*gid));
    mmac[0] = 0x01;
    mmac[1] = 0x00;
    mmac[2] = 0x5e;
    mmac[3] = (SIN(addr)->sin_addr.s_addr >> 8) & 0x7f;
    mmac[4] = (SIN(addr)->sin_addr.s_addr >> 16) & 0xff;
    mmac[5] = (SIN(addr)->sin_addr.s_addr >> 24) & 0xff;
    memcpy (&gid->raw[10], mmac, ETH_ALEN);
}
