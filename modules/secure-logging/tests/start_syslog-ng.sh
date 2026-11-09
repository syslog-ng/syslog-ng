#!/usr/bin/env bash
#############################################################################
# Copyright (c) 2025 Airbus Commercial Aircraft
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2.1 of the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this library; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
#
# As an additional exemption you are allowed to compile & link against the
# OpenSSL libraries as published by the OpenSSL project. See the file
# COPYING for details.
#
#############################################################################

# Author: Airbus Commercial Aircraft <secure-logging@airbus.com>
# File:   start_syslog-ng.sh
# Date:   2026-05-29
#
# Helper to start syslog-ng
# Note: A already running instance is stopped first.

# set -x

VERSION="Version 1.3.3"

SYSLOGNGMAIN="syslog-ng-main"
#SYSLOGNGMAIN="syslog-ng"

# remove path and extension from $0
s=$0
SCRIPTNAME="$(
    b="${s##*/}"
    echo "${b%.*}"
)"
echo "SCRIPTNAME: ${SCRIPTNAME}"

NOW=$(date +%Y-%m-%d_%H%M%S)

echo " "
echo " "
echo "***********************************************************"
echo "*** ${SCRIPTNAME}, ${VERSION}, ${NOW}"
echo "***********************************************************"
echo " "

# --- Usage Check ---
if [[ $# -ne 1 ]]; then
    echo "Usage: $0 <suffix>"
    echo "Example: $0 12345"
    echo " "
    echo "The suffix is used to expand the standard folder for slog test"
    echo "/tmp/test_slog/data"
    echo "/tmp/test_slog/data_<suffix>"
    exit 1
fi

PATH_SUFFIX="$1"

# The string SUBFOLDER must be the same as in syslog-ng.conf
SUBFOLDER="data"
TARGET_PATH="/tmp/test_slog/${SUBFOLDER}_${PATH_SUFFIX}"

# Check if directory exists
if [[ ! -d ${TARGET_PATH} ]]; then
    echo "INFO: '${TARGET_PATH}' is not a valid directory and will be created now."
    mkdir -p "${TARGET_PATH}"
fi
if [[ ! -d ${TARGET_PATH} ]]; then
    echo "ERROR: '${TARGET_PATH}' can not be created!"
    exit 1
fi
# Check if the directory is writable
if [[ ! -w ${TARGET_PATH} ]]; then
    echo "Error: Permission denied. Cannot write to '${TARGET_PATH}'."
    exit 1
fi
# If we reached here, we are good to go!
echo "Given path for syslog-ng is valid. '${TARGET_PATH}' is accessible and writable."

# SCRIPT_DIR=$(cd -- "$(dirname "${BASH_SOURCE[0]}")" && pwd -P)
SCRIPT_DIR="$(cd -- "$(dirname -- "$0")" && pwd -P)"
echo "SCRIPT_DIR: ${SCRIPT_DIR}"

PATH_PREFIX_VALUE=$("${SCRIPT_DIR}"/get_prefix.sh)
PREFIX=${PATH_PREFIX_VALUE}
echo "PREFIX: ${PREFIX}"

START_WAIT_TIME=3
STATS=true
HEALTHCHECK=true
BIN=${PREFIX}/bin
SBIN=${PREFIX}/sbin
ETC=${PREFIX}/etc
VAR=${PREFIX}/var
MACADDRESS="01:23:45:67:89:AB"
SERIALNUMBER="12345678"

# error counter, success when this script returns 0
# cnt_error=0

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
    echo "TARGET_PATH: ${TARGET_PATH}"

    # stop working. exit now.
    exit 1
}

#-----------------------------------------------------------------------
# -- helper function to check of all files in given array do exist
# Usage: all_files_exist "path1" "path2" "path3" ...
check_missing() {
    for path in "$@"; do
        # Check if the path does NOT exist (-e works for files and directories)
        if [[ ! -e ${path} ]]; then
            echo "ERROR: Required path '${path}' not found."
            check_script_config
            # script will exit
        fi
    done
    return 0 # SUCESS, all files found
}

#-----------------------------------------------------------------------
# Helper function to stop running syslog-ng background process
ssn_stop_syslog() {
    if ! "${SCRIPT_DIR}"/stop_syslog-ng.sh; then
        echo "WARNING: Failed to stop syslog-ng"
    fi
}

#-----------------------------------------------------------------------
# Helper function to start syslog-ng background process
ssn_start_syslog() {
    # -- Start background engine syslog-ng -----
    echo " "
    echo "----------------------------------------"
    echo "--- START syslog-ng"
    echo "----------------------------------------"
    echo " "

    cat "${TARGET_PATH}/syslog-ng.conf"

    "${SBIN}"/syslog-ng -f "${TARGET_PATH}/syslog-ng.conf" -Fevd &
    sleep "${START_WAIT_TIME}"
    if pgrep -x "${SYSLOGNGMAIN}" >/dev/null; then
        echo "    syslog-ng has successfully been started"

        if [[ ${STATS} == true ]]; then
            echo "    syslog-ng-ctl stats"
            "${SBIN}"/syslog-ng-ctl stats
        fi

        if [[ ${HEALTHCHECK} == true ]]; then
            echo "    syslog-ng-ctl healtcheck"
            "${SBIN}"/syslog-ng-ctl healthcheck
        fi

    else
        echo "    ERROR: syslog-ng not running. Might be an issue with syslog-ng.conf."
        exit 1
    fi
}

# -- do initial checks -----

check_missing "${TARGET_PATH}" "${PREFIX}" "${BIN}" "${SBIN}"

# -- Ensure syslog-ng engined is not running -----
ssn_stop_syslog

check_missing "${VAR}" "${ETC}" "${SBIN}/syslog-ng" \
    "${SBIN}/syslog-ng-ctl" "${BIN}/slogencrypt" "${BIN}/slogverify" \
    "${BIN}/slogkey" "${BIN}/loggen"

if ! [[ -f "${TARGET_PATH}/host.key" ]]; then
    if ! [[ -f "${TARGET_PATH}/master.key" ]]; then
        # create master.key needed to create host.key
        "${BIN}/slogkey" \
            -m "${TARGET_PATH}/master.key"
        if ! [[ -f "${TARGET_PATH}/master.key" ]]; then
            echo "ERROR! Not found: ${TARGET_PATH}/master.key"
            check_script_config
        fi
        echo "hexdump -C ${TARGET_PATH}/master.key"
        hexdump -C "${TARGET_PATH}/master.key"
    else
        echo " "
        echo "INFO: ${TARGET_PATH}/master.key was already available!"
        echo " "
    fi
    # create derived host.key h0.key
    echo " "
    "${BIN}/slogkey" \
        -d "${TARGET_PATH}/master.key" \
        "${MACADDRESS}" \
        "${SERIALNUMBER}" \
        "${TARGET_PATH}/host.key"
else
    echo " "
    echo "INFO: ${TARGET_PATH}/host.key was already available! Usage count must be taken into account!"
    echo " "
fi

if ! [[ -f "${TARGET_PATH}/syslog-ng.conf" ]]; then
    echo "No syslog-ng conf file found, so a dummy is copied into the folder!"
    SFNCONF="syslog-ng-test-udp-nc.conf"
    cp -f "${SCRIPT_DIR}/${SFNCONF}" "${TARGET_PATH}/syslog-ng.conf"
    sync
    check_missing "${TARGET_PATH}/syslog-ng.conf" "${SCRIPT_DIR}/update_conf_path.sh"
    echo "Update syslog-ng.conf template path .."
    "${SCRIPT_DIR}/update_conf_path.sh" "${TARGET_PATH}/syslog-ng.conf" "${PATH_SUFFIX}" "add"
fi

check_missing "${TARGET_PATH}/host.key"

# ----------------------------------------------------------------------
# START syslog-ng

ssn_start_syslog
