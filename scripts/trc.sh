#! /bin/bash
# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2012-2022 OKTET Labs Ltd.
#
# Helper script to do TRC processing with custom options.
#

source "$(dirname "$(which "$0")")"/guess.sh

. $TE_BASE/scripts/trc.sh --db="${TE_TS_TOPDIR}/trc/top.xml" \
      --key2html="${TE_TS_TOPDIR}/trc/trc.key2html" "$@"
