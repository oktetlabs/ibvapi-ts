/* SPDX-License-Identifier: Apache-2.0
 * Copyright (C) 2012-2022 OKTET Labs Ltd.
 */
/** @page usecases-wc_fields Check that poll_cq() reports events correctly
 *
 * @objective Check that @b poll_cq() correctly sets all fields in
 *            @c ibv_wc structure.
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
 * @param two_qp             If it is @c TRUE create two QPs
 * @param two_cq             If it is @c TRUE create two CQs
 * @param send_first         If it is @c TRUE call @b ibv_post_send()
 *                           before @b ibv_post_recv()
 * @param comp_wr            Describes which WR should be posted
 *
 * @author Yurij Plotnikov <Yurij.Plotnikov@oktetlabs.ru>
 *
 * @par Scenario:
 */

#define TE_TEST_NAME  "usecases/wc_fields"

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
    int                     iut_ibv_port = 0;

    rpc_ptr                 iut_pd = RPC_NULL;

    struct rpc_ibv_mr      *iut_send_mr = NULL;
    struct rpc_ibv_mr      *iut_recv_mr = NULL;

    rpc_ptr                 iut_cq = RPC_NULL;
    rpc_ptr                 aux_cq = RPC_NULL;
    struct rpc_ibv_qp      *iut_qp1 = RPC_NULL;
    struct rpc_ibv_qp      *iut_qp2 = RPC_NULL;

    struct rpc_ibv_context *tst_context = NULL;
    int                     tst_ibv_port = 0;

    rpc_ptr                 tst_pd = RPC_NULL;

    struct rpc_ibv_mr      *tst_mr = NULL;

    rpc_ptr                 tst_cq = RPC_NULL;
    struct rpc_ibv_qp      *tst_qp = RPC_NULL;

    struct rpc_ibv_qp_init_attr qp_attr;

    struct rpc_ibv_qp_attr mod_attr;
    int pool_size = 0;

    const struct sockaddr  *iut_laddr;
    const struct sockaddr  *tst_addr;
    const struct sockaddr  *iut_addr;
    const struct sockaddr  *tst_laddr;
    const struct sockaddr  *iut_mcast_addr = NULL;
    const struct sockaddr  *tst_mcast_addr = NULL;

    struct rpc_ibv_device_attr attr;

    rpc_ptr                 iut_send_buffer;
    rpc_ptr                 iut_recv_buffer;
    rpc_ptr                 tst_buffer;
    void                   *tx_buf;

    uint8_t                 packet[BUF_SIZE];
    int                     pkt_len;

    union rpc_ibv_gid       mgid;

    struct rpc_ibv_sge      sge;
    struct rpc_ibv_recv_wr  recv_wr;
    struct rpc_ibv_recv_wr *recv_bad_wr = NULL;
    struct rpc_ibv_send_wr  send_wr;
    struct rpc_ibv_send_wr *send_bad_wr = NULL;
    struct rpc_ibv_send_wr  tst_wr;
    struct rpc_ibv_send_wr *tst_bad_wr = NULL;
    struct rpc_ibv_wc       wc[MAX_WC_NUM];

    te_bool                 two_qp;
    te_bool                 two_cq;
    te_bool                 send_first;
    const char             *comp_wr;

    te_bool                 is_send[2];

    unsigned int            msg_offset = sizeof(struct ether_header) +
                                         sizeof(struct iphdr) +
                                         sizeof(struct udphdr);

    int i;

    struct rpc_ibv_port_attr   port_attr;

    TEST_START;
    TEST_GET_IBV_PCO(pco_tst);
    TEST_GET_IBV_PCO(pco_iut);
    TEST_GET_ADDR(pco_iut, iut_mcast_addr);
    TEST_GET_ADDR(pco_tst, tst_mcast_addr);
    TEST_GET_ADDR(pco_iut, iut_addr);
    TEST_GET_ADDR(pco_tst, tst_addr);
    TEST_GET_LINK_ADDR(iut_laddr);
    TEST_GET_LINK_ADDR(tst_laddr);
    TEST_GET_BOOL_PARAM(two_qp);
    TEST_GET_BOOL_PARAM(two_cq);
    TEST_GET_BOOL_PARAM(send_first);
    TEST_GET_STRING_PARAM(comp_wr);

    TEST_STEP("Create two buffers @p iut_recv_buffer and @p iut_send_buffer "
              "on @p pco_iut.");
    tx_buf = te_make_buf_by_len(SEND_LEN);
    te_fill_buf(tx_buf, SEND_LEN);

    memset(&attr, 0, sizeof(attr));
    memset(&qp_attr, 0, sizeof(qp_attr));
    memset(&mod_attr, 0, sizeof(mod_attr));
    memset(&send_wr, 0, sizeof(send_wr));
    memset(&recv_wr, 0, sizeof(recv_wr));
    memset(&tst_wr, 0, sizeof(tst_wr));
    memset(&sge, 0, sizeof(sge));
    memset(wc, 0, sizeof(wc));
    memset(is_send, 0, sizeof(is_send));
    memset(&port_attr, 0, sizeof(port_attr));

    iut_recv_buffer = rpc_memalign(pco_iut, TEST_PAGE_SIZE, BUF_SIZE);
    iut_send_buffer = rpc_memalign(pco_iut, TEST_PAGE_SIZE, BUF_SIZE);

    TEST_STEP("Create buffer @p tst_buffer on @p pco_tst.");
    tst_buffer = rpc_memalign(pco_tst, TEST_PAGE_SIZE, BUF_SIZE);

    TEST_STEP("Call @b ibv_open_device() to create device context "
              "on @p pco_iut and @p pco_tst.");
    iut_context = rpc_ibv_open_device(pco_iut, &iut_ibv_port);
    tst_context = rpc_ibv_open_device(pco_tst, &tst_ibv_port);

    TEST_STEP("Call @b ibv_alloc_pd() to create protection domain "
              "on @p pco_iut.");
    iut_pd = rpc_ibv_alloc_pd(pco_iut, iut_context->context);

    TEST_STEP("Call @b ibv_reg_mr() with @p iut_recv_buffer and with "
              "@p iut_send_buffer to create @p iut_recv_mr and "
              "@p iut_send_mr.");
    iut_recv_mr = rpc_ibv_reg_mr(pco_iut, iut_pd, iut_recv_buffer, BUF_SIZE,
                                 IBV_ACCESS_REMOTE_WRITE |
                                 IBV_ACCESS_LOCAL_WRITE |
                                 IBV_ACCESS_REMOTE_READ);
    iut_send_mr = rpc_ibv_reg_mr(pco_iut, iut_pd, iut_send_buffer, BUF_SIZE,
                                 IBV_ACCESS_REMOTE_WRITE |
                                 IBV_ACCESS_LOCAL_WRITE |
                                 IBV_ACCESS_REMOTE_READ);

    TEST_STEP("Call @b ibv_alloc_pd() to create protection domain "
              "on @p pco_tst.");
    tst_pd = rpc_ibv_alloc_pd(pco_tst, tst_context->context);

    TEST_STEP("Call @b ibv_reg_mr() with @p tst_buffer to create @p tst_mr.");
    tst_mr = rpc_ibv_reg_mr(pco_tst, tst_pd, tst_buffer, BUF_SIZE,
                            IBV_ACCESS_REMOTE_WRITE |
                            IBV_ACCESS_LOCAL_WRITE |
                            IBV_ACCESS_REMOTE_READ);

    TEST_STEP("Call @b ibv_query_device() to get device attributes "
              "on @p pco_iut.");
    rpc_ibv_query_device(pco_iut, iut_context->context, &attr);

    pool_size = attr.max_cqe < attr.max_qp_wr ? attr.max_cqe :
                                                attr.max_qp_wr;

    TEST_STEP("Call @b ibv_create_cq() to create one or two completion queues "
              "according to @p two_cq parameter.");
    iut_cq = rpc_ibv_create_cq(pco_iut, iut_context->context, pool_size,
                               RPC_NULL, RPC_NULL, 0);

    if (two_cq)
        aux_cq = rpc_ibv_create_cq(pco_iut, iut_context->context, pool_size,
                                   RPC_NULL, RPC_NULL, 0);

    TEST_STEP("Call @b ibv_create_qp() using created CQs according to device "
              "attributes to create QP on @p pco_iut.");
    qp_attr.send_cq = (two_cq) ? aux_cq : iut_cq;
    qp_attr.recv_cq = iut_cq;
    qp_attr.cap.max_send_wr = pool_size;
    qp_attr.cap.max_recv_wr = pool_size;
    qp_attr.cap.max_send_sge = 1;
    qp_attr.cap.max_recv_sge = 1;
    qp_attr.sq_sig_all = 0;
    qp_attr.qp_type = IBV_QPT_RAW_PACKET;
    iut_qp1 = rpc_ibv_create_qp(pco_iut, iut_pd, &qp_attr);

    TEST_STEP("Move QP from Reset state to @c IBV_QPS_RTS state using "
              "@b ibv_modify_qp().");
    mod_attr.qp_state = IBV_QPS_INIT;
    mod_attr.port_num = iut_ibv_port;
    rpc_ibv_modify_qp(pco_iut, iut_qp1->qp, &mod_attr,
                      IBV_QP_STATE | IBV_QP_PORT);

    mod_attr.qp_state = IBV_QPS_RTR;
    rpc_ibv_modify_qp(pco_iut, iut_qp1->qp, &mod_attr, IBV_QP_STATE);

    mod_attr.qp_state = IBV_QPS_RTS;
    rpc_ibv_modify_qp(pco_iut, iut_qp1->qp, &mod_attr, IBV_QP_STATE);

    rpc_ibv_query_device(pco_tst, tst_context->context, &attr);

    pool_size = attr.max_cqe < attr.max_qp_wr ? attr.max_cqe :
                                                attr.max_qp_wr;

    TEST_STEP("Call @b ibv_create_cq() to create completion queue @p tst_cq.");
    tst_cq = rpc_ibv_create_cq(pco_tst, tst_context->context, pool_size,
                               RPC_NULL, RPC_NULL, 0);

    TEST_STEP("Call @b ibv_create_qp() using @p tst_cq according to device  "
              "attributes to create QP @p tst_qp on @p pco_tst.");
    qp_attr.send_cq = tst_cq;
    qp_attr.recv_cq = tst_cq;
    qp_attr.cap.max_send_wr = pool_size;
    qp_attr.cap.max_recv_wr = pool_size;
    qp_attr.cap.max_send_sge = 1;
    qp_attr.cap.max_recv_sge = 1;
    qp_attr.sq_sig_all = 0;
    qp_attr.qp_type = IBV_QPT_RAW_PACKET;
    tst_qp = rpc_ibv_create_qp(pco_tst, tst_pd, &qp_attr);

    TEST_STEP("Move QP from Reset state to @c IBV_QPS_RTS state using "
              "@b ibv_modify_qp().");
    mod_attr.qp_state = IBV_QPS_INIT;
    mod_attr.port_num = tst_ibv_port;
    rpc_ibv_modify_qp(pco_tst, tst_qp->qp, &mod_attr,
                      IBV_QP_STATE | IBV_QP_PORT);

    mod_attr.qp_state = IBV_QPS_RTR;
    rpc_ibv_modify_qp(pco_tst, tst_qp->qp, &mod_attr, IBV_QP_STATE);

    mod_attr.qp_state = IBV_QPS_RTS;
    rpc_ibv_modify_qp(pco_tst, tst_qp->qp, &mod_attr, IBV_QP_STATE);

    if (two_qp)
    {
        TEST_STEP("If @p two_qp is @c TRUE create the second QP on @p pco_iut and "
                  "move it to @c IBV_QPS_RTS state.");
        qp_attr.send_cq = iut_cq;
        qp_attr.recv_cq = (two_cq) ? aux_cq : iut_cq;
        qp_attr.cap.max_send_wr = pool_size;
        qp_attr.cap.max_recv_wr = pool_size;
        qp_attr.cap.max_send_sge = 1;
        qp_attr.cap.max_recv_sge = 1;
        qp_attr.sq_sig_all = 0;
        qp_attr.qp_type = IBV_QPT_RAW_PACKET;
        iut_qp2 = rpc_ibv_create_qp(pco_iut, iut_pd, &qp_attr);

        mod_attr.qp_state = IBV_QPS_INIT;
        mod_attr.port_num = iut_ibv_port;
        rpc_ibv_modify_qp(pco_iut, iut_qp2->qp, &mod_attr,
                          IBV_QP_STATE | IBV_QP_PORT);

        mod_attr.qp_state = IBV_QPS_RTR;
        rpc_ibv_modify_qp(pco_iut, iut_qp2->qp, &mod_attr, IBV_QP_STATE);

        mod_attr.qp_state = IBV_QPS_RTS;
        rpc_ibv_modify_qp(pco_iut, iut_qp2->qp, &mod_attr, IBV_QP_STATE);
    }

    TEST_STEP("Attach @p iut_qp to multicast group according to @p mcast_addr.");
    ibvts_fill_gid(iut_mcast_addr, &mgid);
    rpc_ibv_attach_mcast(pco_iut, iut_qp1->qp, &mgid, 0);

    TEST_STEP("Call @b ibv_poll_cq() on the first CQ on @p pco_iut and check "
              "that it doesn't report any events.");
    if (rpc_ibv_poll_cq(pco_iut, iut_cq, MAX_WC_NUM, wc) > 0)
        TEST_VERDICT("ibv_poll_cq() reports unexpected event");

    TEST_STEP("Call @b ibv_post_send() or/and @b ibv_post_recv() according to "
              "@p comp_wr and @p send_first parameters. In case of calling "
              "@b ibv_post_send() generate raw multicast packet before and "
              "save it to @p iut_send_buffer.");
    TEST_STEP("In case of posted receive WR generate generate raw multicast "
              "packet, save it to @p tst_buffer and call @b ibv_post_send() "
              "on @p pco_tst.");
    if (strcmp(comp_wr, "recv") == 0 || strcmp(comp_wr, "both") == 0)
    {
        recv_wr.next = NULL;
        recv_wr.sg_list = &sge;
        recv_wr.num_sge = 1;

        sge.length = 1024;
        sge.lkey = iut_recv_mr->lkey;
        sge.addr = iut_recv_buffer;
        recv_wr.wr_id = sge.addr;

        rpc_ibv_post_recv(pco_iut, iut_qp1->qp, &recv_wr, &recv_bad_wr);

        if (rpc_ibv_poll_cq(pco_iut, iut_cq, MAX_WC_NUM, wc) > 0)
            TEST_VERDICT("ibv_poll_cq() reports unexpected event");

        if (!send_first)
        {
            tst_wr.next = NULL;
            tst_wr.sg_list = &sge;
            tst_wr.num_sge = 1;
            tst_wr.opcode = IBV_WR_SEND;
            tst_wr.send_flags = IBV_SEND_IP_CSUM;

            memset(packet, 0, sizeof(packet));
            pkt_len = ibvts_create_raw_udp_dgm(tst_laddr, iut_laddr,
                                               tst_addr, iut_mcast_addr, 0,
                                               TRUE, tx_buf, SEND_LEN,
                                               packet);
            rpc_set_buf_gen(pco_tst, packet, (size_t)pkt_len,
                            tst_buffer, 0);

            sge.length = pkt_len;
            sge.lkey = tst_mr->lkey;
            sge.addr = tst_buffer;
            tst_wr.wr_id = sge.addr;

            rpc_ibv_post_send(pco_tst, tst_qp->qp, &tst_wr, &tst_bad_wr);
        }
    }

    if (strcmp(comp_wr, "send") == 0 || strcmp(comp_wr, "both") == 0)
    {
        send_wr.next = NULL;
        send_wr.sg_list = &sge;
        send_wr.num_sge = 1;
        send_wr.opcode = IBV_WR_SEND;
        send_wr.send_flags = IBV_SEND_IP_CSUM | IBV_SEND_SIGNALED;

        memset(packet, 0, sizeof(packet));
        pkt_len = ibvts_create_raw_udp_dgm(iut_laddr, tst_laddr, iut_addr,
                                           tst_mcast_addr, 0, TRUE, tx_buf,
                                           SEND_LEN, packet);
        rpc_set_buf_gen(pco_iut, packet, (size_t)pkt_len,
                        iut_send_buffer, 0);

        sge.length = pkt_len;
        sge.lkey = iut_send_mr->lkey;
        sge.addr = iut_send_buffer;
        send_wr.wr_id = sge.addr;

        rpc_ibv_post_send(pco_iut, (two_qp)? iut_qp2->qp : iut_qp1->qp,
                          &send_wr, &send_bad_wr);
    }

    if ((strcmp(comp_wr, "recv") == 0 || strcmp(comp_wr, "both") == 0) &&
        send_first)
    {
        tst_wr.next = NULL;
        tst_wr.sg_list = &sge;
        tst_wr.num_sge = 1;
        tst_wr.opcode = IBV_WR_SEND;
        tst_wr.send_flags = IBV_SEND_IP_CSUM;

        memset(packet, 0, sizeof(packet));
        pkt_len = ibvts_create_raw_udp_dgm(tst_laddr, iut_laddr, tst_addr,
                                           iut_mcast_addr, 0, TRUE, tx_buf,
                                           SEND_LEN, packet);
        rpc_set_buf_gen(pco_tst, packet, (size_t)pkt_len,
                        tst_buffer, 0);

        sge.length = pkt_len;
        sge.lkey = tst_mr->lkey;
        sge.addr = tst_buffer;
        tst_wr.wr_id = sge.addr;

        rpc_ibv_post_send(pco_tst, tst_qp->qp, &tst_wr, &tst_bad_wr);
    }

    TEST_STEP("Call @b poll_cq() and check that it returns correct number of  "
              "completed WRs.");
    rc = rpc_ibv_poll_cq(pco_iut, iut_cq, MAX_WC_NUM, wc);

    if (!((rc == 1 && strcmp(comp_wr, "both") != 0) ||
          (rc == 2 && strcmp(comp_wr, "both") == 0)) )
        TEST_VERDICT("poll_cq() returned incorrect number of events");

    TEST_STEP("Check all fields in obtained @c ibv_wc structures array.");
    if (strcmp(comp_wr, "send") == 0 || send_first)
    {
        is_send[0] = TRUE;
        is_send[1] = FALSE;
    }
    else
    {
        is_send[1] = TRUE;
        is_send[0] = FALSE;
    }

    for (i = 0; i < rc; i++)
    {
        if (wc[i].wr_id != (is_send[i] ? send_wr.wr_id : recv_wr.wr_id))
            RING_VERDICT("poll_cq() returned incorrect id for %s WR",
                         is_send[i] ? "send" : "recv");
        if (wc[i].status != IBV_WC_SUCCESS)
            RING_VERDICT("poll_cq() returned not success status for %s WR",
                         is_send[i] ? "send" : "recv");
        if (wc[i].opcode != (is_send[i] ? IBV_WC_SEND : IBV_WC_RECV))
            RING_VERDICT("poll_cq() returned incorrect opcode for %s WR",
                         is_send[i] ? "send" : "recv");
        if (wc[i].byte_len != (is_send[i] ? 0 : SEND_LEN + msg_offset))
            RING_VERDICT("poll_cq() returned incorrect byte_len for %s WR",
                         is_send[i] ? "send" : "recv");
        if ((!two_qp && wc[i].qp_num != iut_qp1->qp_num) ||
            (two_qp && wc[i].qp_num != (is_send[i] ? iut_qp2->qp_num :
                                                     iut_qp1->qp_num)))
            RING_VERDICT("poll_cq() returned incorrect qp_num for %s WR",
                         is_send[i] ? "send" : "recv");
        if (wc[i].wc_flags != (is_send[i] ? 0 : IBV_WC_GRH))
            RING_VERDICT("poll_cq() returned incorrect flags for %s WR",
                         is_send[i] ? "send" : "recv");

        /* FIXME: slid, src_qp and pkey_index fields should be compared
         * with something. 
         */
        if (wc[i].sl != 0 || wc[i].imm_data != 0 ||
            wc[i].dlid_path_bits != 0 ||
            wc[i].vendor_err != 0)
            RING_VERDICT("poll_cq() returned garbage for %s WR",
                         is_send[i] ? "send" : "recv");
    }

    TEST_STEP("Free all allocated resources.");
    rpc_ibv_detach_mcast(pco_iut, iut_qp1->qp, &mgid, 0);

    rpc_ibv_destroy_qp(pco_iut, iut_qp1);
    if (two_qp)
        rpc_ibv_destroy_qp(pco_iut, iut_qp2);
    rpc_ibv_destroy_qp(pco_tst, tst_qp);

    rpc_ibv_destroy_cq(pco_iut, iut_cq);
    if (two_cq)
        rpc_ibv_destroy_cq(pco_iut, aux_cq);
    rpc_ibv_destroy_cq(pco_tst, tst_cq);

    rpc_ibv_dereg_mr(pco_iut, iut_recv_mr);
    rpc_ibv_dereg_mr(pco_iut, iut_send_mr);
    rpc_ibv_dereg_mr(pco_tst, tst_mr);
    rpc_ibv_dealloc_pd(pco_iut, iut_pd);
    rpc_ibv_dealloc_pd(pco_tst, tst_pd);
    rpc_ibv_close_device(pco_iut, iut_context);
    rpc_ibv_close_device(pco_tst, tst_context);

    TEST_SUCCESS;

cleanup:
    free(tx_buf);
    rpc_free(pco_iut, iut_send_buffer);
    rpc_free(pco_iut, iut_recv_buffer);
    rpc_free(pco_tst, tst_buffer);

    TEST_END;
}
