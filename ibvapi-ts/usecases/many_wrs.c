/* SPDX-License-Identifier: Apache-2.0
 * Copyright (C) 2012-2022 OKTET Labs Ltd.
 */
/** @page usecases-many_wrs post_send()/post_recv() operations with many WRs and SGEs
 *
 * @objective Test on reliability of the @b post_send()/ @b post_recv()
 *            operations on @c IBV_QPT_RAW_PACKET QP with many work requests
 *            and non-trivial SGE array.
 *
 * @type use case
 *
 * @param pco_iut            PCO on IUT
 * @param pco_tst            PCO on Tester
 * @param mcast_addr         Multicast address
 * @param tst_addr           Address on @p tst_if
 * @param iut_laddr          Hardware address of @p iut_if
 * @param tst_laddr          Hardware address of @p tst_if
 * @param wrs_num            Number of work requests
 * @param sge_num            Lenght of SGE list
 * @param set_sq_sig_all     If it is @c TRUE set @p sq_sig_all to @c 1
 *                           for QP
 * @param set_signaled       If it is @c TRUE set @c IBV_SEND_SIGNALED in
 *                           send WRs
 * @param set_send_inline    If it is @c TRUE set @c IBV_SEND_INLINE in
 *                           send WRs
 * @param set_ip_csum        If it is @c TRUE set @c IBV_SEND_IP_CSUM in
 *                           send WRs
 *
 * @author Yurij Plotnikov <Yurij.Plotnikov@oktetlabs.ru>
 *
 * @par Scenario:
 */

#define TE_TEST_NAME  "usecases/many_wrs"

#include "ibvapi-test.h"

#define BUF_SIZE 1024
#define SEND_LEN 256
#define MAX_WRS_NUM 16
#define MAX_SGE_NUM 4
#define AUX_BUF_SIZE 20

static void
gen_parts_len(int sge_num, int data_len, int *parts)
{
    int tmp = data_len;
    int i;

    for (i = 0; i < sge_num - 1; i++)
    {
        parts[i] = rand_range(0, tmp);
        tmp -= parts[i];
    }
    parts[sge_num - 1] = tmp;
}

static void
assamble_buf(rcf_rpc_server *rpcs, struct rpc_ibv_sge *sge,
             int sge_num, void *hdr, void *buf)
{
    char  tmp_buf[sizeof(te_eth_ip_udp_hdr) + SEND_LEN + AUX_BUF_SIZE];
    char *ptr = tmp_buf;
    int   i;
    memset(tmp_buf, 0, sizeof(tmp_buf));

    for (i = 0; i < sge_num; i++)
    {
        rpc_get_buf_gen(rpcs, sge[i].addr, 0, sge[i].length,
                        (uint8_t *)ptr);
        ptr += sge[i].length;
    }
    memcpy(hdr, tmp_buf, sizeof(te_eth_ip_udp_hdr));
    memcpy(buf, &tmp_buf[sizeof(te_eth_ip_udp_hdr)], SEND_LEN);
}
int
main(int argc, char *argv[])
{
    rcf_rpc_server     *pco_iut = NULL;
    rcf_rpc_server     *pco_tst = NULL;

    struct rpc_ibv_context *iut_context = NULL;
    struct rpc_ibv_context *tst_context = NULL;
    int                     iut_ibv_port = 0;
    int                     tst_ibv_port = 0;

    rpc_ptr                      iut_pd = RPC_NULL;

    struct rpc_ibv_mr           *iut_mr[MAX_WRS_NUM][MAX_SGE_NUM];
    struct rpc_ibv_comp_channel *iut_ev_ch = NULL;

    rpc_ptr                 iut_scq = RPC_NULL;
    rpc_ptr                 iut_rcq = RPC_NULL;
    rpc_ptr                 tmp_cq = RPC_NULL;
    struct rpc_ibv_qp      *iut_qp = RPC_NULL;

    rpc_ptr                 tst_pd = RPC_NULL;

    struct rpc_ibv_mr           *tst_mr[MAX_WRS_NUM][MAX_SGE_NUM];

    rpc_ptr                 tst_scq = RPC_NULL;
    rpc_ptr                 tst_rcq = RPC_NULL;
    struct rpc_ibv_qp      *tst_qp = RPC_NULL;

    struct rpc_ibv_qp_init_attr    qp_attr;

    struct rpc_ibv_qp_attr mod_attr;
    int pool_size = 0;

    const struct sockaddr  *iut_laddr;
    const struct sockaddr  *tst_addr;
    const struct sockaddr  *tst_laddr;
    const struct sockaddr  *mcast_addr = NULL;

    struct rpc_ibv_device_attr attr;

    rpc_ptr                 iut_buffers[MAX_WRS_NUM][MAX_SGE_NUM];
    rpc_ptr                 tst_buffers[MAX_WRS_NUM][MAX_SGE_NUM];
    void                   *tx_buf[MAX_WRS_NUM];
    void                   *rx_buf;

    uint8_t                 packet[BUF_SIZE];
    int                     pkt_len;

    int                     wrs_num;
    int                     sge_num;

    union rpc_ibv_gid       mgid;

    struct rpc_ibv_sge      recv_sge[MAX_WRS_NUM][MAX_SGE_NUM];
    struct rpc_ibv_sge      send_sge[MAX_WRS_NUM][MAX_SGE_NUM];
    struct rpc_ibv_recv_wr  iut_wr[MAX_WRS_NUM];
    struct rpc_ibv_recv_wr *iut_bad_wr = NULL;
    struct rpc_ibv_send_wr  tst_wr[MAX_WRS_NUM];
    struct rpc_ibv_send_wr *tst_bad_wr = NULL;
    struct rpc_ibv_wc       wc[MAX_WRS_NUM];
    int                     parts[MAX_SGE_NUM];

    struct rpc_pollfd       fds;

    int i;
    int j;
    int send_cnt = 0;

    te_bool                 set_signaled;
    te_bool                 set_sq_sig_all;
    te_bool                 set_send_inline;

    te_eth_ip_udp_hdr    check_pack;

    uint16_t             csum;
    te_bool              correct_csum[MAX_WRS_NUM];
    te_bool              set_ip_csum = FALSE;

    int                  total_len;

    TEST_START;
    TEST_GET_IBV_PCO(pco_tst);
    TEST_GET_IBV_PCO(pco_iut);
    TEST_GET_ADDR(pco_iut, mcast_addr);
    TEST_GET_ADDR(pco_iut, tst_addr);
    TEST_GET_LINK_ADDR(iut_laddr);
    TEST_GET_LINK_ADDR(tst_laddr);
    TEST_GET_INT_PARAM(wrs_num);
    TEST_GET_INT_PARAM(sge_num);
    TEST_GET_BOOL_PARAM(set_sq_sig_all);
    TEST_GET_BOOL_PARAM(set_signaled);
    TEST_GET_BOOL_PARAM(set_send_inline);
    TEST_GET_BOOL_PARAM(set_ip_csum);

    TEST_STEP("Create @p wrs_num * @p sge_num number of buffers on "
              "@p pco_iut and @p pco_tst.");
    for (i = 0; i < wrs_num; i++)
    {
        tx_buf[i] = te_make_buf_by_len(SEND_LEN);
        te_fill_buf(tx_buf[i], SEND_LEN);
    }
    rx_buf = te_make_buf_by_len(SEND_LEN);

    memset(&attr, 0, sizeof(attr));
    memset(&qp_attr, 0, sizeof(qp_attr));
    memset(&mod_attr, 0, sizeof(mod_attr));
    memset(&iut_wr, 0, sizeof(iut_wr));
    memset(&tst_wr, 0, sizeof(tst_wr));
    memset(recv_sge, 0, sizeof(recv_sge));
    memset(send_sge, 0, sizeof(send_sge));
    memset(wc, 0, sizeof(wc));
    memset(iut_mr, 0, sizeof(iut_mr));

    for (i = 0; i < wrs_num; i++)
        for (j = 0; j < sge_num; j++)
        {
            iut_buffers[i][j] = rpc_memalign(pco_iut, TEST_PAGE_SIZE,
                                             BUF_SIZE);
            tst_buffers[i][j] = rpc_memalign(pco_tst, TEST_PAGE_SIZE,
                                             BUF_SIZE);
        }

    TEST_STEP("Call @b ibv_open_device() to create device context "
              "on @p pco_iut.");
    iut_context = rpc_ibv_open_device(pco_iut, &iut_ibv_port);

    TEST_STEP("Call @b ibv_alloc_pd() to create protection domain "
              "on @p pco_iut.");
    iut_pd = rpc_ibv_alloc_pd(pco_iut, iut_context->context);

    TEST_STEP("Create @p wrs_num * @p sge_num number of memory regions using "
              "to allocated buffers on @p pco_iut.");
    for (i = 0; i < wrs_num; i++)
        for (j = 0; j < sge_num; j++)
            iut_mr[i][j] = rpc_ibv_reg_mr(pco_iut, iut_pd,
                                          iut_buffers[i][j],
                                          BUF_SIZE,
                                          IBV_ACCESS_REMOTE_WRITE |
                                          IBV_ACCESS_LOCAL_WRITE |
                                          IBV_ACCESS_REMOTE_READ);

    TEST_STEP("Call @b ibv_query_device() to get device attributes "
              "on @p pco_iut.");
    rpc_ibv_query_device(pco_iut, iut_context->context, &attr);

    pool_size = attr.max_cqe < attr.max_qp_wr ? attr.max_cqe :
                                                attr.max_qp_wr;

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

    TEST_STEP("Call @b ibv_create_qp() using @p iut_rcq, @p iut_scq according "
              "to device attributes to create QP @p iut_qp on @p pco_iut.");
    qp_attr.send_cq = iut_scq;
    qp_attr.recv_cq = iut_rcq;
    qp_attr.cap.max_send_wr = pool_size;
    qp_attr.cap.max_recv_wr = pool_size;
    qp_attr.cap.max_send_sge = sge_num;
    qp_attr.cap.max_recv_sge = sge_num;
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

    TEST_STEP("Create @p wrs_num * @p sge_num number of memory regions using "
              "to allocated buffers on @p pco_tst.");
    for (i = 0; i < wrs_num; i++)
        for (j = 0; j < sge_num; j++)
            tst_mr[i][j] = rpc_ibv_reg_mr(pco_tst, tst_pd,
                                          tst_buffers[i][j],
                                          BUF_SIZE,
                                          IBV_ACCESS_REMOTE_WRITE |
                                          IBV_ACCESS_LOCAL_WRITE |
                                          IBV_ACCESS_REMOTE_READ);

    TEST_STEP("Call @b ibv_query_device() to get device attributes "
              "on @p pco_tst.");
    memset(&attr, 0, sizeof(attr));
    rpc_ibv_query_device(pco_tst, tst_context->context, &attr);

    pool_size = attr.max_cqe < attr.max_qp_wr ? attr.max_cqe :
                                                attr.max_qp_wr;

    TEST_STEP("Call @b ibv_create_cq() to create receive completion queue "
              "@p tst_rcq.");
    tst_rcq = rpc_ibv_create_cq(pco_tst, tst_context->context, pool_size,
                                RPC_NULL, RPC_NULL, 0);

    TEST_STEP("Call @b ibv_create_cq() to create send completion queue "
              "@p tst_scq.");
    tst_scq = rpc_ibv_create_cq(pco_tst, tst_context->context, pool_size,
                                RPC_NULL, RPC_NULL, 0);

    TEST_STEP("Call @b ibv_create_qp() using @p tst_rcq, @p tst_scq "
              "according to device attributes and @p set_sq_sig_all to "
              "create QP @p tst_qp on @p pco_tst.");
    memset(&qp_attr, 0, sizeof(qp_attr));
    qp_attr.send_cq = tst_scq;
    qp_attr.recv_cq = tst_rcq;
    qp_attr.cap.max_send_wr = pool_size;
    qp_attr.cap.max_recv_wr = pool_size;
    qp_attr.cap.max_send_sge = sge_num;
    qp_attr.cap.max_recv_sge = sge_num;
    if (set_send_inline)
        qp_attr.cap.max_inline_data = 500;
    else
        qp_attr.cap.max_inline_data = 0;
    qp_attr.sq_sig_all = (set_sq_sig_all) ? 1 : 0;
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

    TEST_STEP("Call @b ibv_post_recv() on @pco_iut using created memory "
              "regios and buffers and according to @p wrs_num and "
              "@p sge_num parameters on @p pco_iut.");
    for (i = 0; i < wrs_num; i++)
    {
        if (i == wrs_num - 1)
            iut_wr[i].next = NULL;
        else
            iut_wr[i].next = &iut_wr[i + 1];
        iut_wr[i].sg_list = recv_sge[i];
        iut_wr[i].num_sge = sge_num;

        memset(parts, 0, sizeof(parts));
        gen_parts_len(sge_num, sizeof(te_eth_ip_udp_hdr) +
                               SEND_LEN + AUX_BUF_SIZE, parts);
        for (j = 0; j < sge_num; j++)
        {
            recv_sge[i][j].length = parts[j];
            recv_sge[i][j].lkey = iut_mr[i][j]->lkey;
            recv_sge[i][j].addr = iut_buffers[i][j];
        }
        iut_wr[i].wr_id = recv_sge[i][0].addr;
    }

    rpc_ibv_post_recv(pco_iut, iut_qp->qp, iut_wr, &iut_bad_wr);

    TEST_STEP("Call @b ibv_poll_cq() on @p iut_rcq and check that it doesn't "
              "report any events.");
    if (rpc_ibv_poll_cq(pco_iut, iut_rcq, 1, wc) > 0)
        TEST_VERDICT("ibv_poll_cq() reports unexpected event");

    TEST_STEP("Call @b ibv_req_notify_cq() to request completion notification "
              "on @p iut_ev_ch.");
    rpc_ibv_req_notify_cq(pco_iut, iut_rcq, 0);

    TEST_STEP("Call @b poll() on fd of @p iut_ev_ch and check that it "
              "doesn't reports any events.");
    fds.fd = iut_ev_ch->fd;
    fds.events = RPC_POLLIN | RPC_POLLPRI | RPC_POLLERR | RPC_POLLHUP;
    fds.revents = 0;
    if (rpc_poll(pco_iut, &fds, 1, 1000) > 0)
        TEST_VERDICT("poll() reports unexpected event");

    TEST_STEP("Create @p wrs_num number of raw multicast packets and write "
              "them to allocated buffers on @p pco_tst. Each packet would be "
              "devided between @p sge_num buffers.");
#define IBV_SET_FLAG(_flag, _set, _act) \
    do {                                    \
        if (rand_range(0, 1) == 1 && _set)  \
        {                                   \
            { _act; }                       \
            tst_wr[i].send_flags |= _flag;  \
        }                                   \
    } while (0)

    for (i = 0; i < wrs_num; i++)
    {
        correct_csum[i] = FALSE;
        if (i == wrs_num - 1)
            tst_wr[i].next = NULL;
        else
            tst_wr[i].next = &tst_wr[i + 1];
        tst_wr[i].sg_list = send_sge[i];
        tst_wr[i].num_sge = sge_num;
        tst_wr[i].opcode = IBV_WR_SEND;

        if (set_ip_csum)
        {
            tst_wr[i].send_flags = 0;
            IBV_SET_FLAG(IBV_SEND_IP_CSUM, set_ip_csum,
                         { correct_csum[i] = TRUE; });
        }
        else
        {
            tst_wr[i].send_flags = IBV_SEND_IP_CSUM;
            correct_csum[i] = TRUE;
        }
        IBV_SET_FLAG(IBV_SEND_SIGNALED, set_signaled,
                     { send_cnt++; });
        IBV_SET_FLAG(IBV_SEND_INLINE, set_send_inline, { });

        pkt_len = ibvts_create_raw_udp_dgm(tst_laddr, iut_laddr, tst_addr,
                                           mcast_addr, 0, TRUE, tx_buf[i],
                                           SEND_LEN, packet);
        gen_parts_len(sge_num, pkt_len, parts);
        total_len = 0;
        for (j = 0; j < sge_num; j++)
        {
            rpc_set_buf_gen(pco_tst, &packet[total_len], parts[j],
                            tst_buffers[i][j], 0);
            send_sge[i][j].length = parts[j];
            send_sge[i][j].lkey = tst_mr[i][j]->lkey;
            send_sge[i][j].addr = tst_buffers[i][j];
            total_len += parts[j];
        }
        memset(packet, 0, sizeof(packet));
        tst_wr[i].wr_id = send_sge[i][0].addr;
    }
#undef IBV_SET_FLAG

    if (set_sq_sig_all)
        send_cnt = wrs_num;

    TEST_STEP("Call @b ibv_post_send() using allocated buffers and memory "
              "regions on @p pco_tst accoring to test parameters.");
    rpc_ibv_post_send(pco_tst, tst_qp->qp, tst_wr, &tst_bad_wr);
    SLEEP(5);

    if (rpc_ibv_poll_cq(pco_tst, tst_scq, MAX_WRS_NUM, wc) != send_cnt)
        TEST_VERDICT("ibv_poll_cq() on SQ returned incorrect number");

    TEST_STEP("Call @b poll() on fd of @p iut_ev_ch and check that it reports "
              "event.");
    fds.fd = iut_ev_ch->fd;
    fds.events = RPC_POLLIN | RPC_POLLPRI | RPC_POLLERR | RPC_POLLHUP;
    fds.revents = 0;
    if (rpc_poll(pco_iut, &fds, 1, 1000) != 1)
        TEST_VERDICT("poll() doesn't report expected event");

    TEST_STEP("Call @b ibv_poll_cq() on @p iut_rcq and check that it reports "
              "event.");
    memset(wc, 0, sizeof(wc));
    if (rpc_ibv_poll_cq(pco_iut, iut_rcq, MAX_WRS_NUM, wc) == wrs_num)
    {
        rpc_ibv_get_cq_event(pco_iut, iut_ev_ch->cc, &tmp_cq, NULL);
        if (tmp_cq != iut_rcq)
            TEST_VERDICT("ibv_get_cq_event() returns incorrect CQ");
        rpc_ibv_ack_cq_events(pco_iut, tmp_cq, 1);
    }
    else
        TEST_VERDICT("ibv_poll_cq() doesn't report expected events");

    TEST_STEP("Get events from @p iut_rcq and acknowledge it.");
    for (i = 0; i < wrs_num; i++)
    {
        if (wc[i].status != IBV_WC_SUCCESS)
        {
            ERROR("Status of %d work request is %d", i, wc[i].status);
            TEST_VERDICT("Not all WR succeeded");
        }
        memset(rx_buf, 0, SEND_LEN);
        assamble_buf(pco_iut, recv_sge[i], sge_num, &check_pack, rx_buf);

        TEST_STEP("Compare buffers on @p pco_tst and @p pco_iut.");
        if (memcmp(tx_buf[i], rx_buf, SEND_LEN) != 0)
            TEST_VERDICT("Data was corrupted during post_send() and "
                         "post_recv() opterations");

        TEST_STEP("Check that @c IBV_SEND_IP_CSUM, @c IBV_SEND_SIGNALED and "
                  "@c IBV_SEND_INLINE flags are handled correctly.");
        if (correct_csum[i])
        {
            csum = ~check_pack.iphdr.check;
            check_pack.iphdr.check = 0;
            if (csum !=
                calculate_checksum(&(check_pack.iphdr),
                                   sizeof(struct iphdr)))
                TEST_VERDICT("IP checksum is incorrect");
        }
        else if (check_pack.iphdr.check != 0)
            TEST_VERDICT("IP Checksum has been changed but "
                         "IBV_SEND_IP_CSUM was not set");
    }

    TEST_STEP("Free all allocated resources.");
    rpc_ibv_detach_mcast(pco_iut, iut_qp->qp, &mgid, 0);

    rpc_ibv_destroy_qp(pco_iut, iut_qp);

    rpc_ibv_destroy_cq(pco_iut, iut_scq);
    rpc_ibv_destroy_cq(pco_iut, iut_rcq);

    rpc_ibv_destroy_comp_channel(pco_iut, iut_ev_ch);

    for (i = 0; i < wrs_num; i++)
        for (j = 0; j < sge_num; j++)
            rpc_ibv_dereg_mr(pco_iut, iut_mr[i][j]);

    rpc_ibv_dealloc_pd(pco_iut, iut_pd);
    rpc_ibv_close_device(pco_iut, iut_context);

    rpc_ibv_destroy_qp(pco_tst, tst_qp);

    rpc_ibv_destroy_cq(pco_tst, tst_scq);
    rpc_ibv_destroy_cq(pco_tst, tst_rcq);

    for (i = 0; i < wrs_num; i++)
        for (j = 0; j < sge_num; j++)
            rpc_ibv_dereg_mr(pco_tst, tst_mr[i][j]);

    rpc_ibv_dealloc_pd(pco_tst, tst_pd);
    rpc_ibv_close_device(pco_tst, tst_context);

    TEST_SUCCESS;

cleanup:
    free(rx_buf);
    for (i = 0; i < wrs_num; i++)
    {
        free(tx_buf[i]);
        for (j = 0; j < sge_num; j++)
        {
            rpc_free(pco_iut, iut_buffers[i][j]);
            rpc_free(pco_tst, tst_buffers[i][j]);
        }
    }

    TEST_END;
}
