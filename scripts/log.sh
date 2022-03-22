#! /bin/bash
# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2012-2022 OKTET Labs Ltd.
#
# Helper script to generate plain text logs.
#

source "$(dirname "$(which "$0")")"/guess.sh

"${TE_BASE}"/scripts/log.sh "$@"
