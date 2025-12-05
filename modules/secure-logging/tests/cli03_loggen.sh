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

# File:   cli03_loggen.sh
# Author: Airbus Commercial Aircraft <secure-logging@airbus.com>
# Date:   2025-12-05
#
# Smoke Test of cli tools loggen, slogkey, syslog-ng, syslog-ng-cli, slogverify
#
# Needed keys are generated in test by this script as well as all
# log entries.
#
# syslog-ng runs based on a configuration.
# A prepared configuration is copied by this script as
# /tmp/test/slog/syslog-ng.conf.
#
# All binaries referenced will be available after having successfully
# executed `make install`
#
# syslog-ng will be started and stopped several times by
# <by-configure-prefix-and-make-install>/sbin/syslog-ng -f /tmp/test/slog/syslog-ng.conf -Fevd
# and
# <by-configure-prefix-and-make-install>/sbin/syslog-ng-ctl stop
#
# Script artifacts from previous tests are overwritten without asking.
# The destination folder /tmp/test/slog is expected to be used exclusive
# only for one running instance of this script.

VERSION="Version 1.2.3"

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

# SCRIPT_DIR=$(cd -- "$(dirname "${BASH_SOURCE[0]}")" && pwd -P)i
SCRIPT_DIR="$(cd -- "$(dirname -- "$0")" && pwd -P)"
echo "SCRIPT_DIR: ${SCRIPT_DIR}"

PATH_PREFIX_VALUE=$("${SCRIPT_DIR}"/get_prefix.sh)

# This path must fit the one given to configure script with option --prefix
# In case the config.h file has not been found, PREFIX must be set manually.
PREFIX=${PATH_PREFIX_VALUE}
echo "PREFIX: ${PREFIX}"
HOMESLOGTEST=${SCRIPT_DIR}
UDP_PORT=7777
SFNCONF=syslog-ng-test-udp-nc.conf
# e.g.: /home/johndoe/Software/fork/modules/secure-logging/tests/syslog-ng-test-udp-nc.conf
MAX_LOOP=25
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
HOME_BACKUP=${HOME}/${SUBFOLDER_TEST}/${SCRIPTNAME}
COPY_TO_HOME_BACKUP=false
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

    echo "COPY_TO_HOME_BACKUP: ${COPY_TO_HOME_BACKUP}"
    if [ "${COPY_TO_HOME_BACKUP}" = "true" ]; then
        echo "HOME_BACKUP: ${HOME_BACKUP}"
    fi

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

echo " "
echo "#-1"
"${SCRIPT_DIR}"/get_git_info.sh

mkdir -p "${TEST}"
if [ "${COPY_TO_HOME_BACKUP}" = "true" ]; then
    mkdir -p "${HOME_BACKUP}"
    check_missing "${HOME_BACKUP}"
fi

# -- do initial checks -----

check_missing "${TEST}" "${PREFIX}" "${BIN}" "${SBIN}"

# -- Ensure syslog-ng engined is not running -----
stop_syslog

check_missing "${VAR}" "${ETC}" "${HOMESLOGTEST}" "${SBIN}/syslog-ng" "${SBIN}/syslog-ng-ctl" \
    "${BIN}/slogencrypt" "${BIN}/slogverify" "${BIN}/slogkey" "${BIN}/loggen"

# cleanup /tmp/${SUBFOLDER_TEST}/
rm -f "${TEST}"/*.key "${TEST}"/*.dat "${TEST}"/*.txt "${TEST}"/*.chk 2>/dev/null
rm -f "${TEST}"/*.out "${TEST}"/*.log "${TEST}"/*.slo* 2>/dev/null

if [ "${COPY_TO_HOME_BACKUP}" = "true" ]; then
    rm -f "${HOME_BACKUP}"/*.key "${HOME_BACKUP}"/*.dat "${HOME_BACKUP}"/*.txt "${HOME_BACKUP}"/*.chk 2>/dev/null
    rm -f "${HOME_BACKUP}"/*.out "${HOME_BACKUP}"/*.log "${HOME_BACKUP}"/*.slo* 2>/dev/null
fi

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

# Don't create missing inital MAC file mac.dat by touch!!

# -- create log entries in loop by UDP -----
echo " "
echo "#-4"
echo "----------------------------------------"
echo "--- create log msg with loggen via netcat"
echo "----------------------------------------"
echo " "

# -- start syslog-ng engine -----
start_syslog

# loggen -i -D -P -n <Anzahl Logeinträge> 127.0.0.1 7777
echo "${BIN}/loggen -i -D -P -n ${MAX_LOOP} 127.0.0.1 7777"

"${BIN}/loggen" -i -D -P -n "${MAX_LOOP}" "127.0.0.1" "7777"

# -- stop syslog-ng engine -----
echo " "
stop_syslog

echo " "
echo " "
echo "#-5"
echo "----------------------------------------"
echo "--- slogverify NORMAL MODE"
echo "----------------------------------------"
echo " "

echo "${BIN}/slogverify" \
    "--key-file ${TEST}/h0.key" \
    "--mac-file ${TEST}/mac.dat" \
    "${TEST}/messages.slog" \
    "${TEST}/messages_verified.txt"

echo " "
"${BIN}/slogverify" \
    --key-file "${TEST}/h0.key" \
    --mac-file "${TEST}/mac.dat" \
    "${TEST}/messages.slog" \
    "${TEST}/messages_verified.txt" 2>&1 | tee "${TEST}/slogverify-normal-mode-result.log"

# check if output files do exist
echo " "
if ! [ -f "${TEST}/messages.slog" ]; then
    echo "ERROR: ${TEST}/messages.slog does not exist"
    cnt_error=$((cnt_error + 1))
else
    cat "${TEST}/messages.slog"
    echo " "
fi
if ! [ -f "${TEST}/messages_verified.txt" ]; then
    echo "ERROR: ${TEST}/messages_verified.txt does not exist"
    cnt_error=$((cnt_error + 1))
else
    cat "${TEST}/messages_verified.txt"
    echo " "
fi

# [SLOG] ERROR: Log claims to be past entry from past archive. We cannot rewind back to this key without key0. This is going to fail.; entry='2'
# [SLOG] WARNING: Decryption not successful; entry='2'
# [SLOG] WARNING: Unable to recover; entry='3'
# [SLOG] WARNING: Aggregated MAC mismatch. Log might be incomplete;
# [SLOG] WARNING: Aggregated MAC mismatch. Log might be incomplete;
# [SLOG] ERROR: There is a problem with log verification. Please check log manually;

if ! [ -f "${TEST}/slogverify-normal-mode-result.log" ]; then
    printf "ERROR: %s does not exist\n" "${TEST}/slogverify-normal-mode-result.log" >&2
    cnt_error=$((cnt_error + 1))
else
    if grep -q "ERROR: Log claims to be past entry from past archive" "${TEST}/slogverify-normal-mode-result.log"; then
        cnt_error=$((cnt_error + 1))
    fi
    if grep -q "Decryption not successful" "${TEST}/slogverify-normal-mode-result.log"; then
        cnt_error=$((cnt_error + 1))
    fi
    if grep -q "Unable to recover" "${TEST}/slogverify-normal-mode-result.log"; then
        cnt_error=$((cnt_error + 1))
    fi
    if grep -q "Aggregated MAC mismatch" "${TEST}/slogverify-normal-mode-result.log"; then
        cnt_error=$((cnt_error + 1))
    fi
    if grep -q "ERROR: There is a problem with log verification. Please check log manually" "${TEST}/slogverify-normal-mode-result.log"; then
        cnt_error=$((cnt_error + 1))
    fi
fi

echo " "
echo " "
echo "----------------------------------------"
echo "--- Check generated keys, log, mac"
echo "----------------------------------------"
echo " "

# -- key counter -----

echo " "
echo "#-8"
echo "${BIN}/slogkey --counter ${TEST}/h0.key"
"${BIN}/slogkey" --counter "${TEST}/h0.key"

echo " "
echo "${BIN}/slogkey --counter ${TEST}/host.key"
"${BIN}/slogkey" --counter "${TEST}/host.key"

# -- sha256sum -----

echo " "
echo "#-9"
sha256sum "${TEST}/h0.key"
sha256sum "${TEST}/host.key"
sha256sum "${TEST}/mac0.dat"
sha256sum "${TEST}/mac.dat"

# -- hexdump -----

echo " "
echo "#-10"
echo "hexdump -C ${TEST}/h0.key"
hexdump -C "${TEST}/h0.key"

echo " "
echo "hexdump -C ${TEST}/host.key"
hexdump -C "${TEST}/host.key"

echo " "
echo "hexdump -C ${TEST}/mac0.dat"
hexdump -C "${TEST}/mac0.dat"
echo "hexdump -C ${TEST}/mac.dat"
hexdump -C "${TEST}/mac.dat"

if [ "${COPY_TO_HOME_BACKUP}" = "true" ]; then
    # copy from /tmp/test/slog/ to home folder ~/test/<scriptname>/
    cp -R "${TEST}/." "${HOME_BACKUP}/"
    # list content
    echo "#-11"
    echo "ls -al ${HOME_BACKUP}/"
    ls -al "${HOME_BACKUP}/"
fi

echo " "
echo "#-12"
echo "Check if files mac0.dat, mac.dat, h0.key and host.key do exist .."
if ! [ -f "${TEST}/mac0.dat" ]; then
    echo "ERROR: ${TEST}/mac0.dat does not exist"
    cnt_error=$((cnt_error + 1))
fi
if ! [ -f "${TEST}/mac.dat" ]; then
    echo "ERROR: ${TEST}/mac.dat does not exist"
    cnt_error=$((cnt_error + 1))
fi

if ! [ -f "${TEST}/h0.key" ]; then
    echo "ERROR: ${TEST}/h0.key does not exist"
    cnt_error=$((cnt_error + 1))
fi
if ! [ -f "${TEST}/host.key" ]; then
    echo "ERROR: ${TEST}/host.key does not exist"
    cnt_error=$((cnt_error + 1))
fi

# cleanup /tmp/${SUBFOLDER_TEST}/
rm -rf /tmp/"${SUBFOLDER_TEST}"/

echo " "
echo " "
echo "#-13"
echo "----------------------------------------"
echo "--- Used binaries"
echo "----------------------------------------"
echo " "
echo "ls -al ${BIN}/"
ls -al "${BIN}/"
echo " "
echo "ls -al ${SBIN}/"
ls -al "${SBIN}/"
echo " "

sha256sum "${BIN}/slogkey"
sha256sum "${BIN}/slogverify"
sha256sum "${BIN}/slogencrypt"
sha256sum "${BIN}/loggen"
sha256sum "${SBIN}/syslog-ng"
sha256sum "${SBIN}/syslog-ng-ctl"
echo " "

if [ "${COPY_TO_HOME_BACKUP}" = "true" ]; then
    echo "#-14"
    #.txt files are deleted without prompting, .log files remain
    echo "You might want to call this script like:"
    echo "$0 2>&1 | tee ${HOME_BACKUP}/protocol_${SCRIPTNAME}_${NOW}.log"
    echo " "
fi

echo "#-15"
echo "return cnt_error: ${cnt_error}"
#if ((cnt_error == 0)); then
if [ "${cnt_error}" -eq 0 ]; then
    echo "PASS"
else
    echo "Found ERROR"
    echo "FAIL"
fi

echo " "
echo Done
echo " "
# exit "${cnt_error:-0}"
exit "${cnt_error}"
