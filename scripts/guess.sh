#! /bin/bash
# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2012-2022 OKTET Labs Ltd.
#
# Helper script to setup (and guess some) environment variables.
#

fail() {
    echo "$*" >&2
    exit 1
}

TE_TS_RUNDIR="$(pwd -P)"
TE_TS_TOPDIR="$(cd "$(dirname "$(which "$0")")"/.. || exit 1; pwd -P)"
export TE_TS_TOPDIR

if test -z "${TE_BASE}" ; then
    if test -e dispatcher.sh ; then
        export TE_BASE="${TE_TS_RUNDIR}"
    elif test -e "${TE_TS_TOPDIR}/dispatcher.sh" ; then
        export TE_BASE="${TE_TS_TOPDIR}"
    elif test -e "${TE_TS_TOPDIR}/../te/dispatcher.sh" ; then
        export TE_BASE="${TE_TS_TOPDIR}/../te"
    else
        fail "Path to TE sources MUST be specified in TE_BASE"
    fi
fi

if test -z "${TE_BUILD}" ; then
    if test "${TE_TS_TOPDIR}" = "${TE_TS_RUNDIR}" ; then
        TE_BUILD="${TE_TS_TOPDIR}/build"
        mkdir -p build
    else
        TE_BUILD="${TE_TS_RUNDIR}"
    fi
    export TE_BUILD
fi

if test -z "${TE_INSTALL}" ; then
    # If under, TE_INSTALL is always inside TE_BUILD in our case
    TE_INSTALL="${TE_BUILD}"/inst
    export TE_INSTALL
fi

PATH="${PATH}:${TE_INSTALL}/default/bin"
export PATH
LD_LIBRARY_PATH="${TE_INSTALL}/default/lib:${LD_LIBRARY_PATH}"
export LD_LIBRARY_PATH

if test -z "${TSS_CONFDIR}" ; then
    TSS_CONFDIR="${TE_TS_TOPDIR}"/../ts-conf
    if test ! -d "${TSS_CONFDIR}" ; then
        fail "Path to shared ts-conf MUST be specified in TSS_CONFDIR"
    fi
    echo "Guessed TSS_CONFDIR=${TSS_CONFDIR}"
    export TSS_CONFDIR
fi

test -z "${TE_TS_CONFDIR}" -a -d "${TE_TS_TOPDIR}/conf" \
    && TE_TS_CONFDIR="${TE_TS_TOPDIR}/conf"

test -z "${TE_TS_TRC_DB}" -a -e "${TE_TS_TOPDIR}/trc/top.xml" \
    && TE_TS_TRC_DB="${TE_TS_TOPDIR}/trc/top.xml"

test -z "${TE_TS_SRC}" -a -d "${TE_TS_TOPDIR}/ibvapi-ts" \
    && export TE_TS_SRC="${TE_TS_TOPDIR}/ibvapi-ts"

echo TE_TS_SRC=$TE_TS_SRC
