/* SPDX-License-Identifier: Apache-2.0
 * Copyright (C) 2012-2022 OKTET Labs Ltd.
 */
/** @page bnbvalue-bad_qp_type Call ibv_create_qp() with bad QP type
 *
 * @objective Check that @b ibv_create_qp() called with wrong QP type value
 *            returns @c NULL and sets errno to @c EINVAL.
 *
 * @type use case
 *
 * @param pco_iut            PCO on IUT
 *
 * @author Yurij Plotnikov <Yurij.Plotnikov@oktetlabs.ru>
 *
 * @par Scenario:
 */

#define TE_TEST_NAME  "bnbvalue/bad_qp_type"

#include "ibvapi-test.h"

int
main(int argc, char *argv[])
{
    rcf_rpc_server         *pco_iut = NULL;

    struct rpc_ibv_context *iut_context = NULL;

    rpc_ptr                 iut_pd = RPC_NULL;

    rpc_ptr                 iut_scq = RPC_NULL;
    rpc_ptr                 iut_rcq = RPC_NULL;
    struct rpc_ibv_qp      *iut_qp = RPC_NULL;

    struct rpc_ibv_qp_init_attr    qp_attr;
    int                            pool_size = 0;

    struct rpc_ibv_device_attr attr;

    TEST_START;
    TEST_GET_IBV_PCO(pco_iut);

    memset(&attr, 0, sizeof(attr));
    memset(&qp_attr, 0, sizeof(qp_attr));

    TEST_STEP("Call @b ibv_open_device() to create device context "
              "on @p pco_iut.");
    iut_context = rpc_ibv_open_device(pco_iut, NULL);

    TEST_STEP("Call @b ibv_alloc_pd() to create protection domain "
              "on @p pco_iut.");
    iut_pd = rpc_ibv_alloc_pd(pco_iut, iut_context->context);

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

    TEST_STEP("Call @b ibv_create_qp() with @c RPC_INCORRECT_QP_TYPE as QP "
              "type and check that it returns @c NULL and sets errno to "
              "@c EINVAL.");
    qp_attr.send_cq = iut_scq;
    qp_attr.recv_cq = iut_rcq;
    qp_attr.cap.max_send_wr = pool_size;
    qp_attr.cap.max_recv_wr = pool_size;
    qp_attr.cap.max_send_sge = 1;
    qp_attr.cap.max_recv_sge = 1;
    qp_attr.sq_sig_all = 0;
    qp_attr.qp_type = RPC_INCORRECT_QP_TYPE;
    RPC_AWAIT_IUT_ERROR(pco_iut);
    if ((iut_qp = rpc_ibv_create_qp(pco_iut, iut_pd, &qp_attr)) != NULL)
        TEST_VERDICT("QP was created with incorrect type");

    CHECK_RPC_ERRNO(pco_iut, RPC_EINVAL, "ibv_create_qp() with incorrect "
                                         "type returned NULL");

    TEST_STEP("Call @b ibv_create_qp() once again with @c IBV_QPT_RAW_PACKET QP "
              "type and check that it returns correct pointer to QP.");
    qp_attr.qp_type = IBV_QPT_RAW_PACKET;
    iut_qp = rpc_ibv_create_qp(pco_iut, iut_pd, &qp_attr);

    TEST_STEP("Free all allocated resources.");
    TEST_SUCCESS;

cleanup:
    rpc_ibv_destroy_qp(pco_iut, iut_qp);

    rpc_ibv_destroy_cq(pco_iut, iut_scq);
    rpc_ibv_destroy_cq(pco_iut, iut_rcq);

    rpc_ibv_dealloc_pd(pco_iut, iut_pd);

    rpc_ibv_close_device(pco_iut, iut_context);

    TEST_END;
}
