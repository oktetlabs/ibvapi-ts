/* SPDX-License-Identifier: Apache-2.0
 * Copyright (C) 2012-2022 OKTET Labs Ltd.
 */
/** @page bnbvalue-circle_wr post_send/post_recv calls with loop in WR linked list
 *
 * @objective Call @b post_send/ @b post_recv function with loop in
 *            WR linked list and check it correctly reports error.
 *
 * @type conformance, robustness
 *
 * @param pco_iut            PCO on IUT
 * @param use_send_wr        Use @b ibv_post_send() or @b ibv_post_recv()
 *                           function
 * @param circle_len         Length of WR list loop
 *
 * @author Yurij Plotnikov <Yurij.Plotnikov@oktetlabs.ru>
 *
 * @par Scenario:
 */

#define TE_TEST_NAME  "bnbvalue/circle_wr"

#include "ibvapi-test.h"

#define BUF_SIZE 1024
#define MAX_CIRCLE_LEN 8

int
main(int argc, char *argv[])
{
    rcf_rpc_server         *pco_iut = NULL;

    struct rpc_ibv_context *iut_context = NULL;

    rpc_ptr                 iut_pd = RPC_NULL;

    struct rpc_ibv_mr           *iut_mr = NULL;

    rpc_ptr                 iut_scq = RPC_NULL;
    rpc_ptr                 iut_rcq = RPC_NULL;
    struct rpc_ibv_qp      *iut_qp = RPC_NULL;

    struct rpc_ibv_qp_init_attr    qp_attr;

    int pool_size = 0;

    struct rpc_ibv_device_attr attr;

    rpc_ptr                 iut_buffer;

    struct rpc_ibv_sge      sge;
    struct rpc_ibv_recv_wr  recv_wr[MAX_CIRCLE_LEN];
    struct rpc_ibv_recv_wr *recv_bad_wr = NULL;
    struct rpc_ibv_send_wr  send_wr[MAX_CIRCLE_LEN];
    struct rpc_ibv_send_wr *send_bad_wr = NULL;

    te_bool                 use_send_wr;
    int                     i;
    int                     circle_len;

    TEST_START;
    TEST_GET_IBV_PCO(pco_iut);
    TEST_GET_BOOL_PARAM(use_send_wr);
    TEST_GET_INT_PARAM(circle_len);

    memset(&attr, 0, sizeof(attr));
    memset(&qp_attr, 0, sizeof(qp_attr));
    memset(recv_wr, 0, sizeof(recv_wr));
    memset(send_wr, 0, sizeof(send_wr));
    memset(&sge, 0, sizeof(sge));

    TEST_STEP("Create @p iut_buffer buffer on @p pco_iut.");
    iut_buffer = rpc_memalign(pco_iut, TEST_PAGE_SIZE, BUF_SIZE);

    TEST_STEP("Call @b ibv_open_device() to create device context "
              "on @p pco_iut.");
    iut_context = rpc_ibv_open_device(pco_iut, NULL);

    TEST_STEP("Call @b ibv_alloc_pd() to create protection domain "
              "on @p pco_iut.");
    iut_pd = rpc_ibv_alloc_pd(pco_iut, iut_context->context);

    TEST_STEP("Call @b ibv_reg_mr() with @p iut_buffer to create @p iut_mr.");
    iut_mr = rpc_ibv_reg_mr(pco_iut, iut_pd, iut_buffer, BUF_SIZE,
                            IBV_ACCESS_REMOTE_WRITE |
                            IBV_ACCESS_LOCAL_WRITE |
                            IBV_ACCESS_REMOTE_READ);

    TEST_STEP("Call @b ibv_query_device() to get device attributes "
              "on @p pco_iut.");
    rpc_ibv_query_device(pco_iut, iut_context->context, &attr);

    pool_size = attr.max_cqe < attr.max_qp_wr ? attr.max_cqe :
                                                attr.max_qp_wr;

    TEST_STEP("Call @b ibv_create_cq() to create receive completion queue "
              "@p iut_rcq.");
    iut_rcq = rpc_ibv_create_cq(pco_iut, iut_context->context, pool_size,
                                RPC_NULL, RPC_NULL, 0);

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
    TEST_STEP("Create WR linked list with @p circle_len length and with "
              "pointer to the first one in @p next field in last one.");
    TEST_STEP("Call @b ibv_post_send() or @b ibv_post_recv() function "
              "according to @p use_send_wr parameter and check it returns "
              "incorrect WR in @a bad_wr argument.");
    sge.length = 1024;
    sge.lkey = iut_mr->lkey;
    sge.addr = iut_buffer;
    if (use_send_wr)
    {
        for (i = 0; i < circle_len; i++)
        {
            if (i == circle_len - 1)
                send_wr[i].next = NULL;
            else
                send_wr[i].next = &send_wr[i + 1];
            send_wr[i].sg_list = &sge;
            send_wr[i].num_sge = 1;
            send_wr[i].circle_num = 1;

            send_wr[i].opcode = IBV_WR_SEND;
            send_wr[i].send_flags = IBV_SEND_IP_CSUM;

            send_wr[i].wr_id = sge.addr;
        }

        RPC_AWAIT_IUT_ERROR(pco_iut);
        rc = rpc_ibv_post_send(pco_iut, iut_qp->qp, send_wr, &send_bad_wr);
        if (rc != -1)
            TEST_VERDICT("ibv_post_send() returns %d instead of -1", rc);
        else
            CHECK_RPC_ERRNO(pco_iut, RPC_EOK, "ibv_post_send() returns -1");

        for (i = 0; i < circle_len; i++)
            if (&send_wr[i] == send_bad_wr)
            {
                RING_VERDICT("WR #%d is bad", i + 1);
                break;
            }
    }
    else
    {
        for (i = 0; i < circle_len; i++)
        {
            if (i == circle_len - 1)
                recv_wr[i].next = NULL;
            else
                recv_wr[i].next = &recv_wr[i + 1];
            recv_wr[i].sg_list = &sge;
            recv_wr[i].num_sge = 1;
            recv_wr[i].circle_num = 1;

            recv_wr[i].wr_id = sge.addr;
        }

        RPC_AWAIT_IUT_ERROR(pco_iut);
        rc = rpc_ibv_post_recv(pco_iut, iut_qp->qp, recv_wr, &recv_bad_wr);
        if (rc != -1)
            TEST_VERDICT("ibv_post_recv() returns %d instead of -1", rc);
        else
            CHECK_RPC_ERRNO(pco_iut, RPC_EOK, "ibv_post_recv() returns -1");

        for (i = 0; i < circle_len; i++)
            if (&recv_wr[i] == recv_bad_wr)
            {
                RING_VERDICT("WR #%d is bad", i + 1);
                break;
            }
    }

    rpc_ibv_destroy_qp(pco_iut, iut_qp);

    rpc_ibv_destroy_cq(pco_iut, iut_scq);
    rpc_ibv_destroy_cq(pco_iut, iut_rcq);

    rpc_ibv_dereg_mr(pco_iut, iut_mr);
    rpc_ibv_dealloc_pd(pco_iut, iut_pd);
    rpc_ibv_close_device(pco_iut, iut_context);

    TEST_SUCCESS;

cleanup:
    rpc_free(pco_iut, iut_buffer);

    TEST_END;
}
