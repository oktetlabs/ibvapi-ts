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

#include "rcf_rpc.h"

#include "tapi_env.h"
#include "tapi_test.h"

#include "tapi_rpc_signal.h"

#endif /* !__TS_IBVAPI_TEST_H__ */
