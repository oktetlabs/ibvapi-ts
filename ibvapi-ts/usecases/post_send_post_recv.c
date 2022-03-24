/* SPDX-License-Identifier: Apache-2.0
 * Copyright (C) 2012-2022 OKTET Labs Ltd.
 */

/** @page usecases-post_send_post_recv post_send()/post_recv() operations on IBV_QPT_RAW_PACKET QP
 *
 * @objective Test on reliability of the @b post_send()/ @b post_recv()
 *            operations on @c IBV_QPT_RAW_PACKET QP.
 *
 * @type use case
 * 
 * @param pco_iut            PCO on IUT
 * @param pco_tst            PCO on Tester
 * @param mcast_addr         Multicast address
 * @param tst_addr           Address on @p tst_if
 * @param iut_laddr          Hardware address of @p iut_if
 * @param tst_laddr          Hardware address of @p tst_if
 * @param local_write_access Set or don't set IBV_ACCESS_LOCAL_WRITE on IUT
 *                           memory region
 *
 * @author Yurij Plotnikov <Yurij.Plotnikov@oktetlabs.ru>
 *
 * @par Scenario:
 */

#define TE_TEST_NAME  "usecases/post_send_post_recv"

#include "ibvapi-test.h"

#define BUF_SIZE 1024
#define SEND_LEN 256

int
main(int argc, char *argv[])
{
    rcf_rpc_server     *pco_iut = NULL;
    rcf_rpc_server     *pco_tst = NULL;

    int                     tst_s = -1;

    struct rpc_ibv_context *iut_context = NULL;
    struct rpc_ibv_context *tst_context = NULL;
    int                     iut_ibv_port = 0;
    int                     tst_ibv_port = 0;

    rpc_ptr                 iut_pd = RPC_NULL;

    struct rpc_ibv_mr           *iut_mr = NULL;
    struct rpc_ibv_comp_channel *iut_ev_ch = NULL;

    rpc_ptr                 iut_scq = RPC_NULL;
    rpc_ptr                 iut_rcq = RPC_NULL;
    rpc_ptr                 tmp_cq = RPC_NULL;
    struct rpc_ibv_qp      *iut_qp = NULL;

    rpc_ptr                 tst_pd = RPC_NULL;

    struct rpc_ibv_mr           *tst_mr = NULL;

    rpc_ptr                 tst_scq = RPC_NULL;
    rpc_ptr                 tst_rcq = RPC_NULL;
    struct rpc_ibv_qp      *tst_qp = NULL;

    struct rpc_ibv_qp_init_attr    qp_attr;

    struct rpc_ibv_qp_attr mod_attr;
    int pool_size = 0;

    const struct sockaddr  *iut_laddr;
    const struct sockaddr  *tst_addr;
    const struct sockaddr  *tst_laddr;
    const struct sockaddr  *mcast_addr = NULL;

    struct rpc_ibv_device_attr attr;
    struct rpc_ibv_port_attr   port_attr;

    rpc_ptr                 iut_buffer;
    rpc_ptr                 tst_buffer;
    void                   *tx_buf;
    void                   *tmp_buf;
    void                   *rx_buf;

    uint8_t                 packet[BUF_SIZE];
    int                     pkt_len;

    union rpc_ibv_gid       mgid;

    struct rpc_ibv_sge      sge;
    struct rpc_ibv_recv_wr  iut_wr;
    struct rpc_ibv_recv_wr *iut_bad_wr = NULL;
    struct rpc_ibv_send_wr  tst_wr;
    struct rpc_ibv_send_wr *tst_bad_wr = NULL;
    struct rpc_ibv_wc       wc;

    struct tarpc_mreqn      mreq;

    int                     msg_offset = sizeof(struct ether_header) +
                                         sizeof(struct iphdr) +
                                         sizeof(struct udphdr);
    struct rpc_pollfd       fds;

    te_bool                 local_write_access;

    TEST_START;
    TEST_GET_IBV_PCO(pco_tst);
    TEST_GET_IBV_PCO(pco_iut);
    TEST_GET_ADDR(pco_iut, mcast_addr);
    TEST_GET_ADDR(pco_iut, tst_addr);
    TEST_GET_LINK_ADDR(iut_laddr);
    TEST_GET_LINK_ADDR(tst_laddr);
    TEST_GET_BOOL_PARAM(local_write_access);

    TEST_STEP("Create two buffers @p iut_buffer and @p tst_buffer "
              "on @p pco_iut and @p pco_tst.");
    tx_buf = te_make_buf_by_len(SEND_LEN);
    tmp_buf = te_make_buf_by_len(SEND_LEN);
    rx_buf = te_make_buf_by_len(SEND_LEN);
    te_fill_buf(tx_buf, SEND_LEN);

    memset(&attr, 0, sizeof(attr));
    memset(&port_attr, 0, sizeof(port_attr));
    memset(&qp_attr, 0, sizeof(qp_attr));
    memset(&mod_attr, 0, sizeof(mod_attr));
    memset(&iut_wr, 0, sizeof(iut_wr));
    memset(&tst_wr, 0, sizeof(tst_wr));
    memset(&sge, 0, sizeof(sge));
    memset(&wc, 0, sizeof(wc));

    iut_buffer = rpc_memalign(pco_iut, TEST_PAGE_SIZE, BUF_SIZE);
    tst_buffer = rpc_memalign(pco_tst, TEST_PAGE_SIZE, BUF_SIZE);

    TEST_STEP("Create @c SOCK_DGRAM socket on @p pco_tst.");
    tst_s = rpc_socket(pco_tst, RPC_PF_INET, RPC_SOCK_DGRAM,
                       RPC_IPPROTO_UDP);

    TEST_STEP("Set @c IP_MULTICAST_IF option with address of @p tst_if "
              "as parameter.");
    memset(&mreq, 0, sizeof(mreq));
    mreq.type = OPT_IPADDR;
    memcpy(&mreq.address, te_sockaddr_get_netaddr(tst_addr),
           sizeof(mreq.address));
    rpc_setsockopt(pco_tst, tst_s, RPC_IP_MULTICAST_IF, &mreq);

    TEST_STEP("Call @b ibv_open_device() to create device context "
              "on @p pco_iut.");
    iut_context = rpc_ibv_open_device(pco_iut, &iut_ibv_port);

    TEST_STEP("Call @b ibv_alloc_pd() to create protection domain "
              "on @p pco_iut.");
    iut_pd = rpc_ibv_alloc_pd(pco_iut, iut_context->context);

    TEST_STEP("Call @b ibv_reg_mr() with @p iut_buffer and with access "
              "according to @p local_write_access parameter to create "
              "@p iut_mr.");
    if (local_write_access)
        iut_mr = rpc_ibv_reg_mr(pco_iut, iut_pd, iut_buffer, BUF_SIZE,
                                IBV_ACCESS_REMOTE_WRITE |
                                IBV_ACCESS_LOCAL_WRITE |
                                IBV_ACCESS_REMOTE_READ);
    else
        iut_mr = rpc_ibv_reg_mr(pco_iut, iut_pd, iut_buffer, BUF_SIZE,
                                IBV_ACCESS_REMOTE_READ);

    TEST_STEP("Call @b ibv_query_device() to get device attributes "
              "on @p pco_iut.");
    rpc_ibv_query_device(pco_iut, iut_context->context, &attr);

    pool_size = attr.max_cqe < attr.max_qp_wr ? attr.max_cqe :
                                                attr.max_qp_wr;

    TEST_STEP("Call @b ibv_query_port() to get port attributes "
              "on @p pco_iut.");
    rpc_ibv_query_port(pco_iut, iut_context->context, iut_ibv_port,
                       &port_attr);

    TEST_STEP("Call @b ibv_create_comp_channel() to create completion "
              "channel @p iut_ev_ch on @p pco_iut.");
    iut_ev_ch = rpc_ibv_create_comp_channel(pco_iut, iut_context->context);

    TEST_STEP("Call @b ibv_create_cq() with @p iut_ev_ch to create receive "
              "completion queue @p iut_rcq.");
    iut_rcq = rpc_ibv_create_cq(pco_iut, iut_context->context, pool_size,
                                RPC_NULL, iut_ev_ch->cc, 0);

    TEST_STEP("Call @b ibv_create_cq() to create send completion queue "
              "@p iut_scq.");
    iut_scq = rpc_ibv_create_cq(pco_iut, iut_context->context, pool_size,
                                RPC_NULL, RPC_NULL, 0);

    TEST_STEP("Call @b ibv_create_qp() using @p iut_rcq, @p iut_scq "
              "according to device attributes to create QP @p iut_qp "
              "on @p pco_iut.");
    qp_attr.send_cq = iut_scq;
    qp_attr.recv_cq = iut_rcq;
    qp_attr.cap.max_send_wr = pool_size;
    qp_attr.cap.max_recv_wr = pool_size;
    qp_attr.cap.max_send_sge = 1;
    qp_attr.cap.max_recv_sge = 1;
    qp_attr.sq_sig_all = 0;
    qp_attr.qp_type = IBV_QPT_RAW_PACKET;
    iut_qp = rpc_ibv_create_qp(pco_iut, iut_pd, &qp_attr);

    TEST_STEP("Move QP from Reset state to @c IBV_QPS_RTS state using "
              "@b ibv_modify_qp().");
    mod_attr.qp_state = IBV_QPS_INIT;
    mod_attr.port_num = iut_ibv_port;
    rpc_ibv_modify_qp(pco_iut, iut_qp->qp, &mod_attr,
                      IBV_QP_STATE | IBV_QP_PORT);

    mod_attr.qp_state = IBV_QPS_RTR;
    rpc_ibv_modify_qp(pco_iut, iut_qp->qp, &mod_attr, IBV_QP_STATE);

    mod_attr.qp_state = IBV_QPS_RTS;
    rpc_ibv_modify_qp(pco_iut, iut_qp->qp, &mod_attr, IBV_QP_STATE);

    TEST_STEP("Call @b ibv_open_device() to create device context "
              "on @p pco_tst.");
    tst_context = rpc_ibv_open_device(pco_tst, &tst_ibv_port);

    TEST_STEP("Call @b ibv_alloc_pd() to create protection domain "
              "on @p pco_tst.");
    tst_pd = rpc_ibv_alloc_pd(pco_tst, tst_context->context);

    TEST_STEP("Call @b ibv_reg_mr() with @p tst_buffer to create "
              "@p tst_mr.");
    tst_mr = rpc_ibv_reg_mr(pco_tst, tst_pd, tst_buffer, BUF_SIZE,
                            IBV_ACCESS_REMOTE_WRITE |
                            IBV_ACCESS_LOCAL_WRITE |
                            IBV_ACCESS_REMOTE_READ);

    TEST_STEP("Call @b ibv_query_device() to get device attributes "
              "on @p pco_tst.");
    memset(&attr, 0, sizeof(attr));
    rpc_ibv_query_device(pco_tst, tst_context->context, &attr);

    pool_size = attr.max_cqe < attr.max_qp_wr ? attr.max_cqe :
                                                attr.max_qp_wr;

    TEST_STEP("Call @b ibv_query_port() to get port attributes "
              "on @p pco_tst.");
    memset(&port_attr, 0, sizeof(port_attr));
    rpc_ibv_query_port(pco_tst, tst_context->context, tst_ibv_port,
                       &port_attr);
    RING("port_attr.active_mtu = %d", port_attr.active_mtu);

    TEST_STEP("Call @b ibv_create_cq() to create receive completion queue "
              "@p tst_rcq.");
    tst_rcq = rpc_ibv_create_cq(pco_tst, tst_context->context, pool_size,
                                RPC_NULL, RPC_NULL, 0);

    TEST_STEP("Call @b ibv_create_cq() to create send completion queue "
              "@p tst_scq.");
    tst_scq = rpc_ibv_create_cq(pco_tst, tst_context->context, pool_size,
                                RPC_NULL, RPC_NULL, 0);

    TEST_STEP("Call @b ibv_create_qp() using @p tst_rcq, @p tst_scq "
              "according to device attributes to create QP @p tst_qp on "
              "@p pco_tst.");
    memset(&qp_attr, 0, sizeof(qp_attr));
    qp_attr.send_cq = tst_scq;
    qp_attr.recv_cq = tst_rcq;
    qp_attr.cap.max_send_wr = pool_size;
    qp_attr.cap.max_recv_wr = pool_size;
    qp_attr.cap.max_send_sge = 1;
    qp_attr.cap.max_recv_sge = 1;
    qp_attr.sq_sig_all = 0;
    qp_attr.qp_type = IBV_QPT_RAW_PACKET;
    tst_qp = rpc_ibv_create_qp(pco_tst, tst_pd, &qp_attr);

    TEST_STEP("Move QP from Reset state to @c IBV_QPS_RTS state using "
              "@b ibv_modify_qp().");
    memset(&mod_attr, 0, sizeof(mod_attr));
    mod_attr.qp_state = IBV_QPS_INIT;
    mod_attr.port_num = tst_ibv_port;
    rpc_ibv_modify_qp(pco_tst, tst_qp->qp, &mod_attr,
                      IBV_QP_STATE | IBV_QP_PORT);

    mod_attr.qp_state = IBV_QPS_RTR;
    rpc_ibv_modify_qp(pco_tst, tst_qp->qp, &mod_attr, IBV_QP_STATE);

    mod_attr.qp_state = IBV_QPS_RTS;
    rpc_ibv_modify_qp(pco_tst, tst_qp->qp, &mod_attr, IBV_QP_STATE);

    TEST_STEP("Attach @p iut_qp to multicast group according to "
              "@p mcast_addr.");
    ibvts_fill_gid(mcast_addr, &mgid);
    rpc_ibv_attach_mcast(pco_iut, iut_qp->qp, &mgid, 0);

    TEST_STEP("Call @b ibv_post_recv() on @pco_iut using @p iut_mr and "
              "@p iut_buffer.");
    iut_wr.next = NULL;
    iut_wr.sg_list = &sge;
    iut_wr.num_sge = 1;

    sge.length = 1024;
    sge.lkey = iut_mr->lkey;
    sge.addr = iut_buffer;
    iut_wr.wr_id = sge.addr;

    rpc_ibv_post_recv(pco_iut, iut_qp->qp, &iut_wr, &iut_bad_wr);

    TEST_STEP("Call @b ibv_poll_cq() on @p iut_rcq and check that it doesn't "
              "report any events.");
    if (rpc_ibv_poll_cq(pco_iut, iut_rcq, 1, &wc) > 0)
        TEST_VERDICT("ibv_poll_cq() reports unexpected event");

    TEST_STEP("Call @b ibv_req_notify_cq() to request completion "
              "notification on @p iut_ev_ch.");
    rpc_ibv_req_notify_cq(pco_iut, iut_rcq, 0);

    TEST_STEP("Call @b poll() on fd of @p iut_ev_ch and check that it doesn't "
              "reports any events.");
    fds.fd = iut_ev_ch->fd;
    fds.events = RPC_POLLIN | RPC_POLLPRI | RPC_POLLERR | RPC_POLLHUP;
    fds.revents = 0;
    if (rpc_poll(pco_iut, &fds, 1, 1000) > 0)
        TEST_VERDICT("poll() reports unexpected event");

    TEST_STEP("Create and write to @p tst_buffer multicast raw packet using "
              "@p iut_laddr, @p tst_laddr, @p tst_addr and @p mcast_addr.");
    tst_wr.next = NULL;
    tst_wr.sg_list = &sge;
    tst_wr.num_sge = 1;
    tst_wr.opcode = IBV_WR_SEND;
    tst_wr.send_flags = IBV_SEND_IP_CSUM;

    pkt_len = ibvts_create_raw_udp_dgm(tst_laddr, iut_laddr, tst_addr,
                                       mcast_addr, 0, TRUE, tx_buf,
                                       SEND_LEN, packet);
    rpc_set_buf_gen(pco_tst, packet, (size_t)pkt_len, tst_buffer, 0);

    rpc_get_buf_gen(pco_iut, iut_buffer, msg_offset, SEND_LEN,
                    (uint8_t *)rx_buf);

    TEST_STEP("Call @b ibv_post_send() using @p tst_buffer and @p tst_mr "
              "to send multicast packet to @p mcast_addr.");
    sge.length = pkt_len;
    sge.lkey = tst_mr->lkey;
    sge.addr = tst_buffer;
    tst_wr.wr_id = sge.addr;

    rpc_ibv_post_send(pco_tst, tst_qp->qp, &tst_wr, &tst_bad_wr);

    TEST_STEP("Call @b poll() on fd of @p iut_ev_ch and check that it "
              "reports event.");
    fds.fd = iut_ev_ch->fd;
    fds.events = RPC_POLLIN | RPC_POLLPRI | RPC_POLLERR | RPC_POLLHUP;
    fds.revents = 0;
    if (rpc_poll(pco_iut, &fds, 1, 1000) != 1)
        TEST_VERDICT("poll() doesn't report expected event");

    TEST_STEP("Call @b ibv_poll_cq() on @p iut_rcq and check that it "
              "reports event.");
    if (rpc_ibv_poll_cq(pco_iut, iut_rcq, 1, &wc) > 0)
    {
        TEST_STEP("Get events from @p iut_rcq and acknowledge it.");
        rpc_ibv_get_cq_event(pco_iut, iut_ev_ch->cc, &tmp_cq, NULL);
        rpc_ibv_ack_cq_events(pco_iut, tmp_cq, 1);
    }
    else
        TEST_VERDICT("ibv_poll_cq() doesn't report expected event");

    TEST_STEP("Check that payloads of packets in @p iut_buffer and "
              "@p tst_buffer are the same if @p local_write_access is @c TRUE "
              "or @p iut_buffer is not changed if @p local_write_access is "
              "@c FALSE.");
    memcpy(tmp_buf, rx_buf, SEND_LEN);
    rpc_get_buf_gen(pco_iut, iut_buffer, msg_offset, SEND_LEN,
                    (uint8_t *)rx_buf);
    if (local_write_access && memcmp(tx_buf, rx_buf, SEND_LEN) != 0)
        TEST_VERDICT("Data was corrupted during post_send() and "
                     "post_recv() opterations");
    else if (!local_write_access && memcmp(tmp_buf, rx_buf, SEND_LEN) != 0)
        TEST_VERDICT("Data were modified in memory region propected "
                     "from local write.");

    TEST_STEP("Call @b ibv_post_recv() on @pco_iut using @p iut_mr and "
              "@p iut_buffer.");
    iut_wr.next = NULL;
    iut_wr.sg_list = &sge;
    iut_wr.num_sge = 1;

    sge.length = 1024;
    sge.lkey = iut_mr->lkey;
    sge.addr = iut_buffer;
    iut_wr.wr_id = sge.addr;

    rpc_ibv_post_recv(pco_iut, iut_qp->qp, &iut_wr, &iut_bad_wr);

    TEST_STEP("Call @b ibv_req_notify_cq() to request completion "
              "notification on @p iut_ev_ch.");
    rpc_ibv_req_notify_cq(pco_iut, iut_rcq, 0);

    TEST_STEP("Send packet to @p mcast_addr using @b sendto().");
    te_fill_buf(tx_buf, SEND_LEN);
    rpc_sendto(pco_tst, tst_s, tx_buf, SEND_LEN, 0, mcast_addr);

    TEST_STEP("Check that payloads of packets in @p iut_buffer and sent by "
              "@b sendto() are the same if @p local_write_access is @c TRUE "
              "or @p iut_buffer is not changed if @p local_write_access is "
              "@c FALSE.");
    fds.fd = iut_ev_ch->fd;
    fds.events = RPC_POLLIN | RPC_POLLPRI | RPC_POLLERR | RPC_POLLHUP;
    fds.revents = 0;
    if (rpc_poll(pco_iut, &fds, 1, 1000) != 1)
        TEST_VERDICT("poll() doesn't report expected event");

    if (rpc_ibv_poll_cq(pco_iut, iut_rcq, 1, &wc) > 0)
    {
        rpc_ibv_get_cq_event(pco_iut, iut_ev_ch->cc, &tmp_cq, NULL);
        rpc_ibv_ack_cq_events(pco_iut, tmp_cq, 1);
    }
    else
        TEST_VERDICT("ibv_poll_cq() doesn't report expected event");

    rpc_get_buf_gen(pco_iut, iut_buffer, msg_offset, SEND_LEN, (uint8_t *)rx_buf);
    if (local_write_access && memcmp(tx_buf, rx_buf, SEND_LEN) != 0)
        TEST_VERDICT("Data was corrupted during sendto() and "
                     "post_recv() opterations");
    else if (!local_write_access && memcmp(tmp_buf, rx_buf, SEND_LEN) != 0)
        TEST_VERDICT("Data were modified in memory region propected from "
                     "local write.");

    TEST_STEP("Free all allocated resources.");
    rpc_ibv_detach_mcast(pco_iut, iut_qp->qp, &mgid, 0);

    rpc_ibv_destroy_qp(pco_iut, iut_qp);

    rpc_ibv_destroy_cq(pco_iut, iut_scq);
    rpc_ibv_destroy_cq(pco_iut, iut_rcq);

    rpc_ibv_destroy_comp_channel(pco_iut, iut_ev_ch);

    rpc_ibv_dereg_mr(pco_iut, iut_mr);
    rpc_ibv_dealloc_pd(pco_iut, iut_pd);
    rpc_ibv_close_device(pco_iut, iut_context);

    rpc_ibv_destroy_qp(pco_tst, tst_qp);

    rpc_ibv_destroy_cq(pco_tst, tst_scq);
    rpc_ibv_destroy_cq(pco_tst, tst_rcq);

    rpc_ibv_dereg_mr(pco_tst, tst_mr);
    rpc_ibv_dealloc_pd(pco_tst, tst_pd);
    rpc_ibv_close_device(pco_tst, tst_context);

    TEST_SUCCESS;

cleanup:
    free(tx_buf);
    free(rx_buf);
    rpc_free(pco_iut, iut_buffer);
    rpc_free(pco_tst, tst_buffer);

    CLEANUP_RPC_CLOSE(pco_tst, tst_s);

    TEST_END;
}
