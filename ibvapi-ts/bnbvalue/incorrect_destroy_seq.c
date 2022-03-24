/* SPDX-License-Identifier: Apache-2.0
 * Copyright (C) 2012-2022 OKTET Labs Ltd.
 */
/** @page bnbvalue-incorrect_destroy_seq Call dealloc/destroy/detach functions in incorrect order
 *
 * @objective Check that when @b ibv_dealloc_pd(),
 *            @b ibv_destroy_comp_channel(), @b ibv_destroy_cq() or
 *            @b ibv_destroy_qp() function is called in
 *            incorrect sequence it returns appropriate errors.
 *
 * @type use case
 *
 * @param pco_iut            PCO on IUT
 * @param mcast_addr         Multicast address
 * @param test_func          Tested function
 * @param error              Expected error
 *
 * @author Yurij Plotnikov <Yurij.Plotnikov@oktetlabs.ru>
 *
 * @par Scenario:
 */

#define TE_TEST_NAME  "bnbvalue/incorrect_destroy_seq"

#include "ibvapi-test.h"

#define BUF_SIZE 1024
#define SEND_LEN 256

int
main(int argc, char *argv[])
{
    rcf_rpc_server     *pco_iut = NULL;

    struct rpc_ibv_context *iut_context = NULL;
    int                     iut_ibv_port = 0;

    rpc_ptr                 iut_pd = RPC_NULL;

    struct rpc_ibv_comp_channel *iut_ev_ch = NULL;

    rpc_ptr                 iut_rcq = RPC_NULL;
    rpc_ptr                 iut_scq = RPC_NULL;
    struct rpc_ibv_qp      *iut_qp = RPC_NULL;

    struct rpc_ibv_qp_init_attr    qp_attr;

    struct rpc_ibv_qp_attr mod_attr;
    int pool_size = 0;

    const struct sockaddr  *mcast_addr = NULL;

    struct rpc_ibv_device_attr attr;

    union rpc_ibv_gid       mgid;

    const char             *test_func;
    rpc_errno               error;

    TEST_START;
    TEST_GET_IBV_PCO(pco_iut);
    TEST_GET_ADDR(pco_iut, mcast_addr);
    TEST_GET_STRING_PARAM(test_func);
    TEST_GET_ERRNO_PARAM(error);

    memset(&attr, 0, sizeof(attr));
    memset(&qp_attr, 0, sizeof(qp_attr));
    memset(&mod_attr, 0, sizeof(mod_attr));

#define CHECK_TEST_FUNC(_func) \
    do {\
        if (strcmp(test_func, #_func) == 0)\
            goto _func; \
    } while(0)

    TEST_STEP("Call @b ibv_open_device() to create device context "
              "on @p pco_iut.");
    iut_context = rpc_ibv_open_device(pco_iut, &iut_ibv_port);

    TEST_STEP("Call @b ibv_alloc_pd() to create protection domain "
              "on @p pco_iut.");
    iut_pd = rpc_ibv_alloc_pd(pco_iut, iut_context->context);

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
    CHECK_TEST_FUNC(destroy_comp_channel);

    TEST_STEP("If @p test_func is not @b ibv_destroy_comp_channel() call "
              "@b ibv_create_cq() to create send completion queue @p iut_scq.");
    iut_scq = rpc_ibv_create_cq(pco_iut, iut_context->context, pool_size,
                                RPC_NULL, RPC_NULL, 0);

    TEST_STEP("If @p test_func is not @b ibv_destroy_comp_channel() call "
              "@b ibv_create_qp() using @p iut_rcq and @p iut_scq according "
              "to device attributes to create QP @p iut_qp on @p pco_iut.");
    qp_attr.send_cq = iut_scq;
    qp_attr.recv_cq = iut_rcq;
    qp_attr.cap.max_send_wr = pool_size;
    qp_attr.cap.max_recv_wr = pool_size;
    qp_attr.cap.max_send_sge = 1;
    qp_attr.cap.max_recv_sge = 1;
    qp_attr.sq_sig_all = 0;
    qp_attr.qp_type = IBV_QPT_RAW_PACKET;
    iut_qp = rpc_ibv_create_qp(pco_iut, iut_pd, &qp_attr);
    CHECK_TEST_FUNC(destroy_cq);
    CHECK_TEST_FUNC(dealloc_pd);

    TEST_STEP("If @p test_func is @b ibv_destroy_qp Move QP from Reset state "
              "to @c IBV_QPS_RTS state using @b ibv_modify_qp() and then "
              "attach to multicast group according to @p mcast_addr.");
    mod_attr.qp_state = IBV_QPS_INIT;
    mod_attr.port_num = iut_ibv_port;
    rpc_ibv_modify_qp(pco_iut, iut_qp->qp, &mod_attr,
                      IBV_QP_STATE | IBV_QP_PORT);

    mod_attr.qp_state = IBV_QPS_RTR;
    rpc_ibv_modify_qp(pco_iut, iut_qp->qp, &mod_attr, IBV_QP_STATE);

    mod_attr.qp_state = IBV_QPS_RTS;
    rpc_ibv_modify_qp(pco_iut, iut_qp->qp, &mod_attr, IBV_QP_STATE);

    ibvts_fill_gid(mcast_addr, &mgid);
    rpc_ibv_attach_mcast(pco_iut, iut_qp->qp, &mgid, 0);
    CHECK_TEST_FUNC(destroy_qp);

    TEST_STEP("Call @p tested_function and check it returns correct error.");

#define TEST_DESTROY_FUNC(_unlock_action, _func, _func1, _error, \
                          _params...)                                \
    do {                                                             \
_func :                                                              \
        RPC_AWAIT_IUT_ERROR(pco_iut);                                \
        rc = rpc_ibv_##_func(pco_iut, _params);                      \
        if (rc == EBUSY)                                             \
        {                                                            \
            CHECK_RPC_ERRNO(pco_iut, _error,                         \
                            "%s() before %s() "                      \
                            "returned EBUSY", #_func, #_func1);      \
            { _unlock_action }                                       \
            rpc_ibv_##_func(pco_iut, _params);                       \
        }                                                            \
        else if (rc != 0 || strcmp(test_func, #_func) == 0)          \
            TEST_VERDICT("%s() returned incorrect value", #_func);   \
    } while (0)

    TEST_DESTROY_FUNC(
        { rpc_ibv_detach_mcast(pco_iut, iut_qp->qp, &mgid, 0); },
        destroy_qp, detach_mcast, error, iut_qp);

    TEST_DESTROY_FUNC({ rpc_ibv_destroy_qp(pco_iut, iut_qp); },
                      destroy_cq, destroy_qp, error, iut_rcq);
    rpc_ibv_destroy_cq(pco_iut, iut_scq);

    TEST_DESTROY_FUNC({ rpc_ibv_destroy_cq(pco_iut, iut_rcq); },
                      destroy_comp_channel, destroy_cq, error, iut_ev_ch);

    TEST_DESTROY_FUNC({ rpc_ibv_destroy_qp(pco_iut, iut_qp); },
                      dealloc_pd, destroy_qp, error, iut_pd);
    if (strcmp(test_func, "dealloc_pd") == 0)
    {
        rpc_ibv_destroy_cq(pco_iut, iut_rcq);
        rpc_ibv_destroy_cq(pco_iut, iut_scq);
        rpc_ibv_destroy_comp_channel(pco_iut, iut_ev_ch);
    }

    TEST_STEP("Free all allocated resources.");
    rpc_ibv_close_device(pco_iut, iut_context);
#undef TEST_DESTROY_FUNC
#undef CHECK_TEST_FUNC

    TEST_SUCCESS;

cleanup:
    TEST_END;
}
