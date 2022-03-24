/* SPDX-License-Identifier: Apache-2.0
 * Copyright (C) 2012-2022 OKTET Labs Ltd.
 */
/** @file
 * @brief Infiniband Verbs API Test Suite
 *
 * Macros to be used in tests. The header must be included from test
 * sources only. It is allowed to use the macros only from @b main()
 * function of the test.
 *
 * @author Yurij Plotnikov <Yurij.Plotnikov@oktetlabs.ru>
 */

#ifndef __TS_IBVAPI_TEST_H__
#define __TS_IBVAPI_TEST_H__

/* FIXME avoid usage of tested API defines on TEN side */
#include <infiniband/verbs.h>

/* Common test variables */
#ifndef TEST_START_VARS
#define TEST_START_VARS TEST_START_ENV_VARS
#endif

/* The first action in any test - process environment */
#define TEST_START_SPECIFIC TEST_START_ENV

/* Perform environment-related cleanup at the end */
#ifndef TEST_END_SPECIFIC
#define TEST_END_SPECIFIC TEST_END_ENV
#endif

#include "te_config.h"

#include "te_sockaddr.h"

#include "rcf_rpc.h"

#include "tapi_env.h"
#include "tapi_sockaddr.h"
#include "tapi_test.h"

#include "tapi_rpc_misc.h"
#include "tapi_rpc_params.h"
#include "tapi_rpc_rdma_cma.h"
#include "tapi_rpc_signal.h"
#include "tapi_rpc_socket.h"
#include "tapi_rpc_unistd.h"
#include "tapi_rpc_verbs.h"
#include "tapi_rpcsock_macros.h"

#include "ibvapi-ts.h"

/** PAGE size to be used in test */
#define TEST_PAGE_SIZE 4096

/**
 * Open IB library on specified PCO to get IB Verbs from it
 *
 * @param _rpcs   RPC server handler
 * @param _lib    Library name
 */
#define OPEN_IBV_LIB(_rpcs, _lib) \
    do {                                               \
        char    *ibv_lib = NULL;                       \
        ibv_lib = strdup(_lib);                        \
        CHECK_RC(rpc_set_ibv_libname(_rpcs, ibv_lib)); \
        free(ibv_lib);                                 \
    } while (0)

/**
 * Get RPC server and open all needed IB libraries
 *
 * @param _rpcs   RPC server handler
 */
#define TEST_GET_IBV_PCO(_rpcs) \
    do {                                                 \
        TEST_GET_PCO(_rpcs);                             \
        if ((_rpcs) == NULL)                             \
        {                                                \
            TEST_STOP;                                   \
        }                                                \
        OPEN_IBV_LIB(_rpcs, "/usr/lib64/librdmacm.so");  \
    } while (0)

/** Nonexistent QP type */
#define RPC_INCORRECT_QP_TYPE 30

#endif /* !__TS_IBVAPI_TEST_H__ */
