/* SPDX-License-Identifier: Apache-2.0
 * Copyright (C) 2012-2022 OKTET Labs Ltd.
 */
/** @page usecases-cq_context Check that ibv_get_cq_event() returns correct CQ user context
 *
 * @objective Check that @b ibv_get_cq_event() returns the same context in
 *            @c cq_context field as set in @b ibv_create_cq().
 *
 * @type use case
 *
 * @param pco_iut            PCO on IUT
 * @param pco_tst            PCO on Tester
 * @param iut_mcast_addr     Multicast address for IUT
 * @param tst_mcast_addr     Multicast address for tester
 * @param iut_addr           Address on @p iut_if
 * @param tst_addr           Address on @p tst_if
 * @param iut_laddr          Hardware address of @p iut_if
 * @param tst_laddr          Hardware address of @p tst_if
 * @param two_cc             If it is @c TRUE create two CCs
 * @param two_cq             If it is @c TRUE create two CQs
 * @param two_notify_calls   If it is @c TRUE call @b ibv_req_notify_cq()
 *                           for both CQs
 * @param two_contexts       If it is @c TRUE create CQs with different
 *                           contexts
 * @param comp_wr            Describes which WR should be posted
 *
 * @author Yurij Plotnikov <Yurij.Plotnikov@oktetlabs.ru>
 *
 * @par Scenario:
 */

#define TE_TEST_NAME  "usecases/cq_context"

#include "ibvapi-test.h"

#define BUF_SIZE 1024
#define SEND_LEN 256
#define MAX_WC_NUM 4

int
main(int argc, char *argv[])
{
    rcf_rpc_server     *pco_iut = NULL;
    rcf_rpc_server     *pco_tst = NULL;

    struct rpc_ibv_context *iut_context = NULL;
    struct rpc_ibv_context *tst_context = NULL;
    int                     iut_ibv_port = 0;
    int                     tst_ibv_port = 0;

    rpc_ptr                 iut_pd = RPC_NULL;

    struct rpc_ibv_mr           *iut_send_mr = NULL;
    struct rpc_ibv_mr           *iut_recv_mr = NULL;
    struct rpc_ibv_comp_channel *iut_ev_ch1 = NULL;
    struct rpc_ibv_comp_channel *iut_ev_ch2 = NULL;

    rpc_ptr                 iut_cq1 = RPC_NULL;
    rpc_ptr                 iut_cq2 = RPC_NULL;
    rpc_ptr                 tmp_cq = RPC_NULL;
    struct rpc_ibv_qp      *iut_qp = NULL;

    rpc_ptr                 tst_pd = RPC_NULL;

    struct rpc_ibv_mr           *tst_mr = NULL;

    rpc_ptr                 tst_cq = RPC_NULL;
    struct rpc_ibv_qp      *tst_qp = NULL;

    struct rpc_ibv_qp_init_attr    qp_attr;

    struct rpc_ibv_qp_attr mod_attr;
    int pool_size = 0;

    int                    ev_num1 = 0;
    int                    ev_num2 = 0;

    const struct sockaddr  *iut_laddr;
    const struct sockaddr  *tst_addr;
    const struct sockaddr  *iut_addr;
    const struct sockaddr  *tst_laddr;
    const struct sockaddr  *iut_mcast_addr = NULL;
    const struct sockaddr  *tst_mcast_addr = NULL;

    struct rpc_ibv_device_attr attr;
    struct rpc_ibv_port_attr   port_attr;

    rpc_ptr                 iut_recv_buffer;
    rpc_ptr                 iut_send_buffer;
    rpc_ptr                 tst_buffer;
    void                   *tx_buf;

    uint8_t                 packet[BUF_SIZE];
    int                     pkt_len;

    union rpc_ibv_gid       mgid;

    struct rpc_ibv_sge      sge;
    struct rpc_ibv_recv_wr  iut_recv_wr;
    struct rpc_ibv_recv_wr *iut_recv_bad_wr = NULL;
    struct rpc_ibv_send_wr  iut_send_wr;
    struct rpc_ibv_send_wr *iut_send_bad_wr = NULL;
    struct rpc_ibv_send_wr  tst_wr;
    struct rpc_ibv_send_wr *tst_bad_wr = NULL;
    struct rpc_ibv_wc       wc[MAX_WC_NUM];

    te_bool                 two_cq;
    te_bool                 two_cc;
    te_bool                 two_notify_calls;
    te_bool                 two_contexts;
    const char             *comp_wr;

    rpc_ptr                 cq_context1;
    rpc_ptr                 cq_context2;
    rpc_ptr                 tmp_cq_context;

    TEST_START;
    TEST_GET_IBV_PCO(pco_tst);
    TEST_GET_IBV_PCO(pco_iut);
    TEST_GET_ADDR(pco_iut, iut_mcast_addr);
    TEST_GET_ADDR(pco_tst, tst_mcast_addr);
    TEST_GET_ADDR(pco_iut, iut_addr);
    TEST_GET_ADDR(pco_tst, tst_addr);
    TEST_GET_LINK_ADDR(iut_laddr);
    TEST_GET_LINK_ADDR(tst_laddr);
    TEST_GET_BOOL_PARAM(two_cq);
    TEST_GET_BOOL_PARAM(two_cc);
    TEST_GET_BOOL_PARAM(two_notify_calls);
    TEST_GET_BOOL_PARAM(two_contexts);
    TEST_GET_STRING_PARAM(comp_wr);

    TEST_STEP("Create two buffers @p iut_recv_buffer and @p iut_send_buffer "
              "on @p pco_iut.");

    tx_buf = te_make_buf_by_len(SEND_LEN);
    te_fill_buf(tx_buf, SEND_LEN);

    memset(&attr, 0, sizeof(attr));
    memset(&port_attr, 0, sizeof(port_attr));
    memset(&qp_attr, 0, sizeof(qp_attr));
    memset(&mod_attr, 0, sizeof(mod_attr));
    memset(&iut_recv_wr, 0, sizeof(iut_recv_wr));
    memset(&iut_send_wr, 0, sizeof(iut_send_wr));
    memset(&tst_wr, 0, sizeof(tst_wr));
    memset(&sge, 0, sizeof(sge));
    memset(wc, 0, sizeof(wc));

    iut_send_buffer = rpc_memalign(pco_iut, TEST_PAGE_SIZE, BUF_SIZE);
    iut_recv_buffer = rpc_memalign(pco_iut, TEST_PAGE_SIZE, BUF_SIZE);

    TEST_STEP("Create buffer @p tst_buffer on @p pco_tst");
    tst_buffer = rpc_memalign(pco_tst, TEST_PAGE_SIZE, BUF_SIZE);
    cq_context1 = rpc_malloc(pco_iut, BUF_SIZE);
    if (two_contexts)
        cq_context2 = rpc_malloc(pco_iut, BUF_SIZE);

    TEST_STEP("Create one or two CQ contexts according to @p two_contexts "
              "parameter.");
    iut_context = rpc_ibv_open_device(pco_iut, &iut_ibv_port);

    TEST_STEP("Call @b ibv_alloc_pd() to create protection domain on "
              "@p pco_iut.");
    iut_pd = rpc_ibv_alloc_pd(pco_iut, iut_context->context);

    TEST_STEP("Call @b ibv_reg_mr() with @p iut_recv_buffer and with "
              "@p iut_send_buffer to create @p iut_recv_mr and @p iut_send_mr.");
    iut_send_mr = rpc_ibv_reg_mr(pco_iut, iut_pd, iut_send_buffer, BUF_SIZE,
                                 IBV_ACCESS_REMOTE_WRITE |
                                 IBV_ACCESS_LOCAL_WRITE |
                                 IBV_ACCESS_REMOTE_READ);
    iut_recv_mr = rpc_ibv_reg_mr(pco_iut, iut_pd, iut_recv_buffer, BUF_SIZE,
                                 IBV_ACCESS_REMOTE_WRITE |
                                 IBV_ACCESS_LOCAL_WRITE |
                                 IBV_ACCESS_REMOTE_READ);

    TEST_STEP("Call @b ibv_query_device() to get device attributes "
              "on @p pco_iut.");
    rpc_ibv_query_device(pco_iut, iut_context->context, &attr);

    pool_size = attr.max_cqe < attr.max_qp_wr ? attr.max_cqe :
                                                attr.max_qp_wr;

    rpc_ibv_query_port(pco_iut, iut_context->context, iut_ibv_port,
                       &port_attr);

    TEST_STEP("Call @b ibv_create_comp_channel() to create one or two "
              "completion channels according to @p two_cc on @p pco_iut.");
    iut_ev_ch1 = rpc_ibv_create_comp_channel(pco_iut, iut_context->context);
    if (two_cc)
        iut_ev_ch2 = rpc_ibv_create_comp_channel(pco_iut,
                                                 iut_context->context);

    TEST_STEP("Call @b ibv_create_cq() to create one or two completion "
              "queues according to @p two_cq parameter and using one or "
              "two created CCs.");
    iut_cq1 = rpc_ibv_create_cq(pco_iut, iut_context->context, pool_size,
                                cq_context1, iut_ev_ch1->cc, 0);
    if (two_cq)
        iut_cq2 = rpc_ibv_create_cq(pco_iut, iut_context->context,
                                    pool_size,
                                    (two_contexts) ? cq_context2 :
                                                     cq_context1,
                                    (two_cc) ? iut_ev_ch2->cc :
                                               iut_ev_ch1->cc,
                                    0);

    TEST_STEP("Call @b ibv_create_qp() using created CQs according to device "
              "attributes to create QP on @p pco_iut.");
    qp_attr.send_cq = (two_cq) ? iut_cq2 : iut_cq1;
    qp_attr.recv_cq = iut_cq1;
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

    TEST_STEP("Call @b ibv_open_device() to create device context on "
              "@p pco_tst.");
    tst_context = rpc_ibv_open_device(pco_tst, &tst_ibv_port);

    TEST_STEP("Call @b ibv_alloc_pd() to create protection domain on "
              "@p pco_tst.");
    tst_pd = rpc_ibv_alloc_pd(pco_tst, tst_context->context);

    TEST_STEP("Call @b ibv_reg_mr() with @p tst_buffer to create "
              "@p tst_mr.");
    tst_mr = rpc_ibv_reg_mr(pco_tst, tst_pd, tst_buffer, BUF_SIZE,
                            IBV_ACCESS_REMOTE_WRITE |
                            IBV_ACCESS_LOCAL_WRITE |
                            IBV_ACCESS_REMOTE_READ);

    TEST_STEP("Call @b ibv_query_device() to get device attributes on "
              "@p pco_tst.");
    memset(&attr, 0, sizeof(attr));
    rpc_ibv_query_device(pco_tst, tst_context->context, &attr);

    pool_size = attr.max_cqe < attr.max_qp_wr ? attr.max_cqe :
                                                attr.max_qp_wr;

    memset(&port_attr, 0, sizeof(port_attr));
    rpc_ibv_query_port(pco_tst, tst_context->context, tst_ibv_port,
                       &port_attr);
    RING("port_attr.active_mtu = %d", port_attr.active_mtu);

    TEST_STEP("Call @b ibv_create_cq() to create completion queue @p tst_cq.");
    tst_cq = rpc_ibv_create_cq(pco_tst, tst_context->context, pool_size,
                               RPC_NULL, RPC_NULL, 0);

    TEST_STEP("Call @b ibv_create_qp() using @p tst_cq according to device "
              "attributes to create QP @p tst_qp on @p pco_tst.");
    memset(&qp_attr, 0, sizeof(qp_attr));
    qp_attr.send_cq = tst_cq;
    qp_attr.recv_cq = tst_cq;
    qp_attr.cap.max_send_wr = pool_size;
    qp_attr.cap.max_recv_wr = pool_size;
    qp_attr.cap.max_send_sge = 1;
    qp_attr.cap.max_recv_sge = 1;
    qp_attr.sq_sig_all = 0;
    qp_attr.qp_type = IBV_QPT_RAW_PACKET;
    tst_qp = rpc_ibv_create_qp(pco_tst, tst_pd, &qp_attr);

    TEST_STEP("Call @b ibv_req_notify_cq() on both or on first CQs according "
              "to @p two_notify_calls parameter.");
    memset(&mod_attr, 0, sizeof(mod_attr));
    mod_attr.qp_state = IBV_QPS_INIT;
    mod_attr.port_num = tst_ibv_port;
    rpc_ibv_modify_qp(pco_tst, tst_qp->qp, &mod_attr,
                      IBV_QP_STATE | IBV_QP_PORT);

    mod_attr.qp_state = IBV_QPS_RTR;
    rpc_ibv_modify_qp(pco_tst, tst_qp->qp, &mod_attr, IBV_QP_STATE);

    mod_attr.qp_state = IBV_QPS_RTS;
    rpc_ibv_modify_qp(pco_tst, tst_qp->qp, &mod_attr, IBV_QP_STATE);

    rpc_ibv_req_notify_cq(pco_iut, iut_cq1, 0);
    if (two_notify_calls)
        rpc_ibv_req_notify_cq(pco_iut, iut_cq2, 0);

    TEST_STEP("Attach @p iut_qp to multicast group according to @p mcast_addr.");
    ibvts_fill_gid(iut_mcast_addr, &mgid);
    rpc_ibv_attach_mcast(pco_iut, iut_qp->qp, &mgid, 0);

    TEST_STEP("Call @b ibv_post_send() or/and @b ibv_post_recv() according "
              "to @p comp_wr parameter. In case of calling @b ibv_post_send() "
              "generate raw multicast packet before and save it to "
              "@p iut_send_buffer.");
    TEST_STEP("In case of posted receive WR generate generate raw multicast "
              "packet, save it to @p tst_buffer and call @b ibv_post_send() "
              "on @p pco_tst.");
    if (strcmp(comp_wr, "send") != 0)
    {
        iut_recv_wr.next = NULL;
        iut_recv_wr.sg_list = &sge;
        iut_recv_wr.num_sge = 1;

        sge.length = 1024;
        sge.lkey = iut_recv_mr->lkey;
        sge.addr = iut_recv_buffer;
        iut_recv_wr.wr_id = sge.addr;

        rpc_ibv_post_recv(pco_iut, iut_qp->qp, &iut_recv_wr,
                          &iut_recv_bad_wr);
        tst_wr.next = NULL;
        tst_wr.sg_list = &sge;
        tst_wr.num_sge = 1;
        tst_wr.opcode = IBV_WR_SEND;
        tst_wr.send_flags = IBV_SEND_IP_CSUM;

        pkt_len = ibvts_create_raw_udp_dgm(tst_laddr, iut_laddr, tst_addr,
                                           iut_mcast_addr, 0, TRUE, tx_buf,
                                           SEND_LEN, packet);
        rpc_set_buf_gen(pco_tst, packet, (size_t)pkt_len, tst_buffer, 0);

        sge.length = pkt_len;
        sge.lkey = tst_mr->lkey;
        sge.addr = tst_buffer;
        tst_wr.wr_id = sge.addr;

        rpc_ibv_post_send(pco_tst, tst_qp->qp, &tst_wr, &tst_bad_wr);

    }

    if (strcmp(comp_wr, "recv") != 0)
    {
        iut_send_wr.next = NULL;
        iut_send_wr.sg_list = &sge;
        iut_send_wr.num_sge = 1;
        iut_send_wr.opcode = IBV_WR_SEND;
        iut_send_wr.send_flags = IBV_SEND_IP_CSUM | IBV_SEND_SIGNALED;

        pkt_len = ibvts_create_raw_udp_dgm(iut_laddr, tst_laddr, iut_addr,
                                           tst_mcast_addr, 0, TRUE, tx_buf,
                                           SEND_LEN, packet);
        rpc_set_buf_gen(pco_tst, packet, (size_t)pkt_len, iut_send_buffer,
                        0);

        sge.length = pkt_len;
        sge.lkey = iut_send_mr->lkey;
        sge.addr = iut_send_buffer;
        iut_send_wr.wr_id = sge.addr;

        rpc_ibv_post_send(pco_iut, iut_qp->qp, &iut_send_wr,
                          &iut_send_bad_wr);
    }

    TEST_STEP("Call @b ibv_poll_cq() on one or two CQs and check it returns "
              "correct number of events.");
    ev_num1 = rpc_ibv_poll_cq(pco_iut, iut_cq1, MAX_WC_NUM, wc);
    if (!((!two_cq && (strcmp(comp_wr, "recv") == 0 ||
                       strcmp(comp_wr, "send") == 0) && ev_num1 == 1) ||
          (!two_cq && strcmp(comp_wr, "both") == 0 && ev_num1 == 2) ||
          (two_cq && strcmp(comp_wr, "send") != 0 && ev_num1 == 1) ||
          (two_cq && strcmp(comp_wr, "send") == 0 && ev_num1 == 0)))
        RING_VERDICT("Incorrect number of events on the first CQ.");
    if (two_cq)
    {
        ev_num2 = rpc_ibv_poll_cq(pco_iut, iut_cq2, MAX_WC_NUM, wc);
        if (!((strcmp(comp_wr, "recv") != 0 && ev_num2 == 1) ||
              (strcmp(comp_wr, "recv") == 0 && ev_num2 == 0)))
            RING_VERDICT("Incorrect number of events on the second CQ.");
    }

    TEST_STEP("Call @b get_cq_event() one or two CQs and check it returns "
              "correct CQ and CQ context.");
    TEST_STEP("Ack all obtainted events.");
    if (ev_num1 > 0)
    {
        rpc_ibv_get_cq_event(pco_iut, iut_ev_ch1->cc, &tmp_cq,
                             &tmp_cq_context);
        if (tmp_cq != iut_cq1)
            TEST_VERDICT("get_cq_event() returned incorrect CQ.");
        if (tmp_cq_context != cq_context1)
            RING_VERDICT("get_cq_event() returned incorrect CQ context.");
        rpc_ibv_ack_cq_events(pco_iut, tmp_cq, 1);
    }
    if (ev_num2 > 0 && two_notify_calls)
    {
        rpc_ibv_get_cq_event(pco_iut, (two_cc) ? iut_ev_ch2->cc :
                                                 iut_ev_ch1->cc,
                             &tmp_cq, &tmp_cq_context);
        if (tmp_cq != iut_cq2)
            TEST_VERDICT("get_cq_event() returned incorrect CQ.");
        if (tmp_cq_context != ((two_contexts) ? cq_context2 : cq_context1))
            RING_VERDICT("get_cq_event() returned incorrect CQ context.");
        rpc_ibv_ack_cq_events(pco_iut, tmp_cq, 1);
    }

    TEST_STEP("Free all allocated resources.");

    rpc_ibv_detach_mcast(pco_iut, iut_qp->qp, &mgid, 0);

    rpc_ibv_destroy_qp(pco_iut, iut_qp);

    rpc_ibv_destroy_cq(pco_iut, iut_cq1);
    if (two_cq)
        rpc_ibv_destroy_cq(pco_iut, iut_cq2);

    rpc_ibv_destroy_comp_channel(pco_iut, iut_ev_ch1);
    if (two_cc)
        rpc_ibv_destroy_comp_channel(pco_iut, iut_ev_ch2);

    rpc_ibv_dereg_mr(pco_iut, iut_send_mr);
    rpc_ibv_dereg_mr(pco_iut, iut_recv_mr);
    rpc_ibv_dealloc_pd(pco_iut, iut_pd);
    rpc_ibv_close_device(pco_iut, iut_context);

    rpc_ibv_destroy_qp(pco_tst, tst_qp);

    rpc_ibv_destroy_cq(pco_tst, tst_cq);

    rpc_ibv_dereg_mr(pco_tst, tst_mr);
    rpc_ibv_dealloc_pd(pco_tst, tst_pd);
    rpc_ibv_close_device(pco_tst, tst_context);

    TEST_SUCCESS;

cleanup:
    free(tx_buf);
    rpc_free(pco_iut, iut_send_buffer);
    rpc_free(pco_iut, iut_recv_buffer);
    rpc_free(pco_iut, cq_context1);
    rpc_free(pco_iut, cq_context2);
    rpc_free(pco_tst, tst_buffer);

    TEST_END;
}
