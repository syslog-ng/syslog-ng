#!/bin/sh
############################################################################
# Copyright (c) 2025 Airbus Commercial Aircraft
#
# This program is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License version 2 as published
# by the Free Software Foundation, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
#
# As an additional exemption you are allowed to compile & link against the
# OpenSSL libraries as published by the OpenSSL project. See the file
# COPYING for details.
#
#############################################################################

# File:   start_syslog-ng.sh
# Author: Airbus Commercial Aircraft <secure-logging@airbus.com>
# Date:   2025-12-05
#
# Helper to start syslog-ng
# Note: A already running instance is stopped first.

# set -x

VERSION="Version 1.0.5"
# remove path and extension from $0
s=$0
SCRIPTNAME="$(
    b="${s##*/}"
    echo "${b%.*}"
)"
NOW=$(date +%Y-%m-%d_%H%M_%S)
echo " "
echo " "
echo "***********************************************************"
echo "*** ${SCRIPTNAME}, ${VERSION}, ${NOW}"
echo "***********************************************************"
echo " "

# SCRIPT_DIR=$(cd -- "$(dirname "${BASH_SOURCE[0]}")" && pwd -P)
SCRIPT_DIR="$(cd -- "$(dirname -- "$0")" && pwd -P)"
echo "SCRIPT_DIR: ${SCRIPT_DIR}"

PATH_PREFIX_VALUE=$("${SCRIPT_DIR}"/get_prefix.sh)

# This path must fit the one given to configure script with option --prefix
# In case the config.h file has not been found, PREFIX must be set manually.
PREFIX=${PATH_PREFIX_VALUE}
echo "PREFIX: ${PREFIX}"

HOMESLOGTEST=${SCRIPT_DIR}
echo "HOMESLOGTEST: ${HOMESLOGTEST}"

UDP_PORT=7777
SFNCONF=syslog-ng-test-udp-nc.conf
# e.g.: /home/johndoe/Software/fork/modules/secure-logging/tests/syslog-ng-test-udp-nc.conf
START_WAIT_TIME=3
STOP_WAIT_TIME=2
STATS=true
HEALTHCHECK=true
BIN=${PREFIX}/bin
SBIN=${PREFIX}/sbin
ETC=${PREFIX}/etc
VAR=${PREFIX}/var
SUBFOLDER=slog
SUBFOLDER_TEST="test"
TEST=/tmp/${SUBFOLDER_TEST}/${SUBFOLDER}
MACADDRESS="01:23:45:67:89:AB"
SERIALNUMBER="12345678"

# error counter, success when this script returns 0
cnt_error=0

#-----------------------------------------------------------------------
# list current configuration and exit with error
check_script_config() {
    echo "ERROR! Precondition to start failed. Check configuration of $0"
    echo " "

    # Prefix is provided by a script. When this is not working
    # User can try to set it manually.
    echo "PREFIX: ${PREFIX}"

    echo "BIN: ${BIN}"
    echo "SBIN: ${SBIN}"
    echo "ETC: ${ETC}"
    echo "VAR: ${VAR}"
    echo "TEST: ${TEST}"
    echo "SFNCONF: ${SFNCONF}"
    echo "UDP_PORT: ${UDP_PORT}"

    # stop working. exit now.
    exit 1
}

#-----------------------------------------------------------------------
# -- helper function to check of all files in given array do exist
# Usage: all_files_exist "path1" "path2" "path3" ...
check_missing() {
    for path in "$@"; do
        # Check if the path does NOT exist (-e works for files and directories)
        if [ ! -e "${path}" ]; then
            echo "Error: Required path '${path}' not found."
            check_script_config
            return 1 # ERROR, safe, check_script_config already exit 1 when file not found
        fi
    done
    return 0 # SUCESS, all files found
}

#-----------------------------------------------------------------------
# Helper function to stop running syslog-ng background process
stop_syslog() {
    echo " "
    echo "----------------------------------------"
    echo "--- STOP syslog-ng"
    echo "----------------------------------------"
    echo " "

    # stop running instance of syslog-ng
    if pgrep -x "syslog-ng" >/dev/null; then
        echo "    syslog-ng is running and will be stopped now"
        # stop old instance
        "${SBIN}/syslog-ng-ctl" stop
        sleep "${STOP_WAIT_TIME}"
    fi

    if pgrep -x "syslog-ng" >/dev/null; then
        echo "    ERROR! syslog-ng still running!"
        cnt_error=$((cnt_error + 1))
    else
        echo "    sylog-ng is not active"
    fi
}

#-----------------------------------------------------------------------
# Helper function to start syslog-ng background process
start_syslog() {
    # -- Start background engine syslog-ng -----
    echo " "
    echo "----------------------------------------"
    echo "--- START syslog-ng"
    echo "----------------------------------------"
    echo " "
    "${SBIN}"/syslog-ng -f "${TEST}"/syslog-ng.conf -Fevd &
    sleep "${START_WAIT_TIME}"
    if pgrep -x "syslog-ng" >/dev/null; then
        echo "    syslog-ng has successfully been started"

        if [ "${STATS}" = true ]; then
            echo "    syslog-ng-ctl stats"
            "${SBIN}"/syslog-ng-ctl stats
        fi

        if [ "${HEALTHCHECK}" = true ]; then
            echo "    syslog-ng-ctl healtcheck"
            "${SBIN}"/syslog-ng-ctl healthcheck
        fi

    else
        echo "    ERROR! syslog-ng not running"
        exit 1
    fi
}

mkdir -p "${TEST}"
# -- do initial checks -----

check_missing "${TEST}" "${PREFIX}" "${BIN}" "${SBIN}"

# -- Ensure syslog-ng engined is not running -----
stop_syslog

check_missing "${VAR}" "${ETC}" "${HOMESLOGTEST}" "${SBIN}/syslog-ng" \
    "${SBIN}/syslog-ng-ctl" "${BIN}/slogencrypt" "${BIN}/slogverify" \
    "${BIN}/slogkey" "${BIN}/loggen"

if ! [ -f "${TEST}/host.key" ]; then
    if ! [ -f "${TEST}/master.key" ]; then
        # create master.key needed to create host.key
        echo " "
        echo "#-2 Create master key"
        echo "${BIN}/slogkey" \
            "-m ${TEST}/master.key"
        "${BIN}/slogkey" \
            -m "${TEST}/master.key"
        if ! [ -f "${TEST}/master.key" ]; then
            echo "ERROR! Not found: ${TEST}/master.key"
            check_script_config
        fi
        echo "hexdump -C ${TEST}/master.key"
        hexdump -C "${TEST}/master.key"
    else
        echo " "
        echo "INFO: ${TEST}/master.key was already available!"
        echo " "
    fi
    # create derived host.key h0.key
    echo " "
    echo "#-3 Create host key h0.key"
    echo "${BIN}/slogkey" \
        "-d ${TEST}/master.key" \
        "${MACADDRESS}" \
        "${SERIALNUMBER}" \
        "${TEST}/h0.key"

    "${BIN}/slogkey" \
        -d "${TEST}/master.key" \
        "${MACADDRESS}" \
        "${SERIALNUMBER}" \
        "${TEST}/h0.key"
    cp "${TEST}"/h0.key "${TEST}"/host.key
else
    echo " "
    echo "INFO: ${TEST}/host.key was already available! Usage count must be taken into account!"
    echo " "
fi

check_missing "${TEST}/h0.key" "${TEST}/host.key" "${HOMESLOGTEST}/${SFNCONF}"

cp -f "${HOMESLOGTEST}/${SFNCONF}" "${TEST}/syslog-ng.conf"

check_missing "${TEST}/syslog-ng.conf"

# simple quick check whether udp port is found in conf file
if grep -q "${UDP_PORT}" "${TEST}/syslog-ng.conf"; then
    printf "Found UDP port %s in %s/syslog-ng.conf\n" "${UDP_PORT}" "${TEST}"
else
    printf "ERROR! Not found: UDP port %s in %s/syslog-ng.conf\n" "${UDP_PORT}" "${TEST}" >&2
    exit 1
fi

# ----------------------------------------------------------------------
# START syslog-ng

start_syslog
