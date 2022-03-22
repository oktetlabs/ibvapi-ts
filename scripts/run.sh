#! /bin/bash
# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2012-2022 OKTET Labs Ltd.
#
# Helper script to run Test Environment for the Test Suite.
#

set -e
source "$(dirname "$(which "$0")")"/guess.sh
source ${TSS_CONFDIR}/scripts/lib.run

run_fail() {
    echo "$*" >&2
    exit 1
}

usage() {
    cat <<EOF
USAGE: run.sh [run.sh options] [dispatcher.sh options]
Options:
  --no-item                 Do not try to reserve requested
                            configuration with item
                            (should be used before --cfg
                             to take effect).
  --cfg=<CFG>               Configuration to be used.
  --no-reuse-pco            Restart RPC servers in each test (it makes
                            testing slower, but avoids inheritance)

EOF
    "${TE_BASE}"/dispatcher.sh --help
    exit 1
}

# Reuse PCO by default to run tests faster
export TE_ENV_REUSE_PCO=yes

declare -a RUN_OPTS
declare -a GEN_OPTS

do_item=true

while test -n "$1" ; do
    case "$1" in
        --help) usage ;;

        --no-item) do_item=false ;;

        --cfg=*)
            cfg="${1#--cfg=}"
            RUN_OPTS+=("--opts=run/$cfg")

            if $do_item; then
                take_items "$cfg"
            fi

            ;;

        --reuse-pco)
            export TE_ENV_REUSE_PCO=yes
            ;;

        --no-reuse-pco)
            export TE_ENV_REUSE_PCO=no
            ;;

        *)  RUN_OPTS+=("$1") ;;
    esac
    shift 1
done

RUN_OPTS+=(--opts=opts.ts)

GEN_OPTS+=(--conf-dirs="${TE_TS_CONFDIR}:${TSS_CONFDIR}")

GEN_OPTS+=(--trc-db="${TE_TS_TRC_DB}")
GEN_OPTS+=(--trc-comparison=normalised)
GEN_OPTS+=(--trc-html=trc-brief.html)
GEN_OPTS+=(--trc-no-expected)
GEN_OPTS+=(--trc-no-total --trc-no-unspec)
GEN_OPTS+=(--build-meson)
GEN_OPTS+=(--tester-only-req-logues)

"${TE_BASE}"/dispatcher.sh "${GEN_OPTS[@]}" "${RUN_OPTS[@]}"
RESULT=$?

if test ${RESULT} -ne 0 ; then
    echo FAIL
    echo ""
fi

echo -ne "\a"
exit ${RESULT}
