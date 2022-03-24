/* SPDX-License-Identifier: Apache-2.0
 * Copyright (C) 2012-2022 OKTET Labs Ltd.
 */
/** @page bnbvalue-bad_cq_pool_size Call ibv_create_cq() function with bad pool_size argument
 *
 * @objective Check that @b ibv_create_cq() function called with incorrect
 *            value in @a pool_size argument returns @c NULL and sets errno
 *            to @c EINVAL.
 *
 * @type use case
 *
 * @param pco_iut            PCO on IUT
 *
 * @author Yurij Plotnikov <Yurij.Plotnikov@oktetlabs.ru>
 *
 * @par Scenario:
 */

#define TE_TEST_NAME  "bnbvalue/bad_cq_pool_size"

#include "ibvapi-test.h"

int
main(int argc, char *argv[])
{
    rcf_rpc_server         *pco_iut = NULL;

    struct rpc_ibv_context *iut_context = NULL;
    rpc_ptr                 iut_cq = RPC_NULL;

    int                     pool_size = 0;

    struct rpc_ibv_device_attr attr;

    TEST_START;
    TEST_GET_IBV_PCO(pco_iut);

    memset(&attr, 0, sizeof(attr));

    TEST_STEP("Call @b ibv_open_device() to create device context on @p pco_iut.");

    TEST_STEP("Call @b ibv_query_device() to get device attributes "
              "on @p pco_iut.");
    rpc_ibv_query_device(pco_iut, iut_context->context, &attr);
    pool_size = attr.max_cqe < attr.max_qp_wr ? attr.max_cqe :
                                                attr.max_qp_wr;

    TEST_STEP("Call @b ibv_create_cq() with @c -1 in @a pool_size argument "
              "and check it returns @c NULL and sets errno to @c EINVAL.");
    iut_context = rpc_ibv_open_device(pco_iut, NULL);
    RPC_AWAIT_IUT_ERROR(pco_iut);
    if ((iut_cq = rpc_ibv_create_cq(pco_iut, iut_context->context, -1,
                                    RPC_NULL, RPC_NULL, 0)) != RPC_NULL)
        TEST_VERDICT("CQ was created with incorrect pool size");

    CHECK_RPC_ERRNO(pco_iut, RPC_EINVAL, "ibv_create_cq() with incorrect "
                                         "pool size returned NULL");

    TEST_STEP("Call @b ibv_create_cq() once again with correct value in "
              "@a pool_size argument and check it returns correct pointer to CQ.");
    iut_cq = rpc_ibv_create_cq(pco_iut, iut_context->context, pool_size,
                               RPC_NULL, RPC_NULL, 0);

    TEST_STEP("Free all allocated resources.");
    TEST_SUCCESS;

cleanup:
    rpc_ibv_destroy_cq(pco_iut, iut_cq);

    rpc_ibv_close_device(pco_iut, iut_context);

    TEST_END;
}
