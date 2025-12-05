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

# File:   stop_syslog-ng.sh
# Author: Airbus Commercial Aircraft <secure-logging@airbus.com>
# Date:   2025-12-05
#
# Helper to stop syslog-ng

VERSION="Version 1.0.4"
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

SCRIPT_DIR="$(cd -- "$(dirname -- "$0")" && pwd -P)"
echo "SCRIPT_DIR: ${SCRIPT_DIR}"
PATH_PREFIX_VALUE=$("${SCRIPT_DIR}"/get_prefix.sh)
# This path must fit the one given to configure script with option --prefix
# In case the config.h file has not been found, PREFIX must be set manually.
PREFIX=${PATH_PREFIX_VALUE}
echo "PREFIX: ${PREFIX}"
STOP_WAIT_TIME=2
BIN=${PREFIX}/bin
SBIN=${PREFIX}/sbin

# error counter, success when this script returns 0
cnt_error=0

#-----------------------------------------------------------------------
# list current configuration and exit with error
check_script_config() {
    echo "ERROR! Precondition failed. Check configuration of $0"
    echo " "

    # Prefix is provided by a script. When this is not working
    # User can try to set it manually.
    echo "PREFIX: ${PREFIX}"
    echo "BIN: ${BIN}"
    echo "SBIN: ${SBIN}"
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

check_missing "${PREFIX}" "${SBIN}"
check_missing "${SBIN}/syslog-ng" "${SBIN}/syslog-ng-ctl"

# ----------------------------------------------------------------------
# STOP syslog-ng

stop_syslog
