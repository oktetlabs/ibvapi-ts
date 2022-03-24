/* SPDX-License-Identifier: Apache-2.0
 * Copyright (C) 2012-2022 OKTET Labs Ltd.
 */
/** @file
 * @brief InfiniBand Verbs API Test Suite
 *
 * Epilogue script.
 *
 * @author Yurij Plotnikov <Yurij.Plotnikov@oktetlabs.ru>
 */

/** Logging subsystem entity name */
#define TE_TEST_NAME    "epilogue"

#include "ibvapi-test.h"

/**
 * Stop corruption engine, if necessary.
 *
 * @param pco_iut       PCO on IUT
 */
static void
stop_corruption_engine(rcf_rpc_server *pco_iut)
{
    tarpc_pid_t pid;

    if (cfg_get_instance_fmt(NULL, (void *)&pid,
                             "/local:/corruption_pid:") == 0)
    {
        RPC_AWAIT_IUT_ERROR(pco_iut);
        rpc_ta_kill_death(pco_iut, pid);
        cfg_del_instance_fmt(FALSE, "/local:/corruption_pid:");
    }
}

/**
 * Cleanup.
 *
 * @retval EXIT_SUCCESS     success
 * @retval EXIT_FAILURE     failure
 */
int
main(int argc, char **argv)
{
    rcf_rpc_server *pco_iut;

    TEST_START;

    TEST_GET_PCO(pco_iut);

    stop_corruption_engine(pco_iut);

    TEST_SUCCESS;

cleanup:

    TEST_END;
}
