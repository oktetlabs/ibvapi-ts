/* SPDX-License-Identifier: Apache-2.0
 * Copyright (C) 2012-2022 OKTET Labs Ltd.
 */
/** @page bnbvalue-create_cq_bad_comp_vect Call ibv_create_cq() with incorrect comp_vector
 *
 * @objective Check that @b ibv_create_cq() function called with negative
 *            or too big @a comp_vector argument returns @c NULL and sets
 *            errno to @c EINVAL.
 *
 * @type use case
 *
 * @param pco_iut            PCO on IUT
 * @param num_cv             Number of completion vector to be passed to
 *                           @b ibv_create_cq() function
 *
 * @author Yurij Plotnikov <Yurij.Plotnikov@oktetlabs.ru>
 *
 * @par Scenario:
 */

#define TE_TEST_NAME  "bnbcalue/create_cq_bad_comp_vect"

#include "ibvapi-test.h"

int
main(int argc, char *argv[])
{
    rcf_rpc_server     *pco_iut = NULL;

    struct rpc_ibv_context *iut_context = NULL;
    rpc_ptr                 iut_pd = RPC_NULL;

    rpc_ptr                 iut_cq = RPC_NULL;

    int pool_size = 0;

    struct rpc_ibv_device_attr attr;

    const char                *num_cv;

    /*
     * Test preambule.
     */
    TEST_START;
    TEST_GET_IBV_PCO(pco_iut);
    TEST_GET_STRING_PARAM(num_cv);

    memset(&attr, 0, sizeof(attr));

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

    TEST_STEP("Call @b ibv_create_cq() with too big or negative value in "
              "@p comp_vector according to @p num_cv paramter. Check it "
              "returns @c NULL and sets errno to @c EINVAL.");
    RPC_AWAIT_IUT_ERROR(pco_iut);
    if (strcmp(num_cv, "negative") == 0)
        iut_cq = rpc_ibv_create_cq(pco_iut, iut_context->context,
                                   pool_size, RPC_NULL, RPC_NULL, -1);
    else if (strcmp(num_cv, "big") == 0)
        iut_cq = rpc_ibv_create_cq(pco_iut, iut_context->context,
                                   pool_size, RPC_NULL, RPC_NULL,
                                   iut_context->num_comp_vectors);
    else
        TEST_FAIL("Incorrect value of 'num_cv' parameter");

    if (iut_cq != RPC_NULL)
        TEST_VERDICT("ibv_create_cq() succeed with %s value of "
                     "comp_vector argument", num_cv);
    CHECK_RPC_ERRNO(pco_iut, RPC_EINVAL,
                    "ibv_create_cq() with %s value of comp_vector "
                    "argument returns NULL", num_cv);

    TEST_STEP("Call @b ibv_create_cq() with correct number in "
              "@p comp_vector and check that it returns correct pointer "
              "to CQ.");
    iut_cq = rpc_ibv_create_cq(pco_iut, iut_context->context,
                               pool_size, RPC_NULL, RPC_NULL,
                               iut_context->num_comp_vectors - 1);

    TEST_STEP("Free all allocated resources.");

    rpc_ibv_destroy_cq(pco_iut, iut_cq);

    rpc_ibv_dealloc_pd(pco_iut, iut_pd);

    rpc_ibv_close_device(pco_iut, iut_context);

    TEST_SUCCESS;

cleanup:
    TEST_END;
}
