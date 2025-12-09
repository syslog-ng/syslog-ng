#!/bin/sh
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
# File:   cli06_syslog.sh
# Date:   2025-12-09
#
# Smoke Test of cli tools slogkey, syslog-ng, syslog-ng-cli, slogverify
# In this test syslog-ng is only stopped after all log entries have been
# triggered, so syslog-ng is not restarted after each log entry.
# Iterative verification is therefore not possible, due to lack of
# separate keys and macs, but this scenario is a more realistic one.
# Needed keys are generated in the test by this script as well as all
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
# Log entries are triggered by netcat and UDP this way:
# cat plain.txt | nc -u 127.0.0.1 7777
# or
# nc -u 127.0.0.1 7777 < plain.txt
#
# Script artifacts from previous tests are overwritten without asking.
# The destination folder /tmp/test/slog is expected to be used exclusive
# only for one running instance of this script.

set -x

VERSION="Version 1.1.0"

# remove path and extension from $0
s=$0
SCRIPTNAME="$(
    b="${s##*/}"
    echo "${b%.*}"
)"
echo "SCRIPTNAME: ${SCRIPTNAME}"

PID=$$
echo "PID: ${PID}"

RANDOM_ID=$(
    /bin/dd if=/dev/urandom bs=1 count=4 2>/dev/null |
    od -An -N4 -tx
)
CLEAN_ID=$(echo "${RANDOM_ID}" | tr -d ' ')

NOW=$(date +%Y-%m-%d_%H%M%S)
echo " "
echo " "
echo "***********************************************************"
echo "*** ${SCRIPTNAME}, ${VERSION}, ${NOW}"
echo "***********************************************************"
echo " "

# -- When there is a parameter given, no matter what, KEEP_DATA is set to TRUE
KEEP_DATA=false
if [ ! "$#" -eq 0 ]; then
    KEEP_DATA=true
fi
echo "KEEP_DATA: ${KEEP_DATA}"

# NOT in POSIX: SCRIPT_DIR=$(cd -- "$(dirname "${BASH_SOURCE[0]}")" && pwd -P)
SCRIPT_DIR="$(cd -- "$(dirname -- "$0")" && pwd -P)"
echo "SCRIPT_DIR: ${SCRIPT_DIR}"

PATH_PREFIX_VALUE=$("${SCRIPT_DIR}"/get_prefix.sh)
# This path must fit the one given to the build system where binaries are provided.
PREFIX=${PATH_PREFIX_VALUE}
echo "PREFIX: ${PREFIX}"
if [ ! -e "${PREFIX}" ]; then
    echo "Error: Required path PREFIX for binaries: '${PREFIX}' not found."
    exit 1
fi
HOMESLOGTEST=${SCRIPT_DIR}

UDP_PORT=7777
SFNCONF=syslog-ng-test-udp-nc.conf
# e.g.: /home/johndoe/Software/fork/modules/secure-logging/tests/syslog-ng-test-udp-nc.conf
# The default path in conf is: mypath "/tmp/test_slog/data"

MAX_LOOP=9
START_WAIT_TIME=3
STOP_WAIT_TIME=2
STATS=true
HEALTHCHECK=true
BIN=${PREFIX}/bin
SBIN=${PREFIX}/sbin
ETC=${PREFIX}/etc
VAR=${PREFIX}/var
SUBFOLDER_TEST="test_slog"
PATH_SUFFIX="${SCRIPTNAME}_${PID}_${CLEAN_ID}"
SUBFOLDER="data"
TEST=/tmp/${SUBFOLDER_TEST}/${SUBFOLDER}_${PATH_SUFFIX}
echo "TEST: ${TEST}"

HOME_BACKUP=${HOME}/${SUBFOLDER_TEST}/${SCRIPTNAME}
COPY_TO_HOME_BACKUP="false"
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
    # User can try to set it manually its the place where binaries are provided.
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

check_missing "${VAR}" "${ETC}" "${HOMESLOGTEST}" "${SBIN}/syslog-ng" "${SBIN}/syslog-ng-ctl"
check_missing "${BIN}/slogencrypt" "${BIN}/slogverify" "${BIN}/slogkey" "${BIN}/loggen"

# cleanup
if [ "${KEEP_DATA}" = true ]; then
    echo " "
    echo "INFO: Data from previous test is not deleted!"
    echo "INFO: Keys are re-used!"
    echo " "
else
    rm -f "${TEST}"/*.key "${TEST}"/*.dat "${TEST}"/*.txt "${TEST}"/*.chk 2>/dev/null
    rm -f "${TEST}"/*.out "${TEST}"/*.log "${TEST}"/*.slo* 2>/dev/null

    if [ "${COPY_TO_HOME_BACKUP}" = "true" ]; then
        rm -f "${HOME_BACKUP}"/*.key "${HOME_BACKUP}"/*.dat "${HOME_BACKUP}"/*.txt "${HOME_BACKUP}"/*.chk 2>/dev/null
        rm -f "${HOME_BACKUP}"/*.out "${HOME_BACKUP}"/*.log "${HOME_BACKUP}"/*.slo* 2>/dev/null
    fi
fi


# config working path in syslog-ng.conf

cp -f "${HOMESLOGTEST}/${SFNCONF}" "${TEST}/syslog-ng.conf"
check_missing "${TEST}/syslog-ng.conf" "${SCRIPT_DIR}/update_conf_path.sh"
echo "Update syslog-ng.conf template path .."
RETVALUC=$("${SCRIPT_DIR}/update_conf_path.sh" "${TEST}/syslog-ng.conf" "${PATH_SUFFIX}" "add")
echo "RETVALUC: ${RETVALUC}"


# Generate Key or check existing

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


# simple quick check whether udp port is found in conf file
if grep -q "${UDP_PORT}" "${TEST}/syslog-ng.conf"; then
    printf "Found UDP port %s in %s/syslog-ng.conf\n" "${UDP_PORT}" "${TEST}"
else
    printf "ERROR! Not found: UDP port %s in %s/syslog-ng.conf\n" "${UDP_PORT}" "${TEST}" >&2
    exit 1
fi

# If host key was not created newly, the usage count must be used
# as offset for naming h<n>.key and mac<n>.dat.
# The offset MUST also be considered when doing iterative verification.
echo "${BIN}/slogkey --counter ${TEST}/host.key"
"${BIN}"/slogkey --counter "${TEST}/host.key" 2>&1 | tee "${TEST}/host-key-counter.log"
counter_value=$(grep 'counter=' "${TEST}/host-key-counter.log" | cut -d'=' -f2)
echo "Host.key counter: ${counter_value}"

# -- create log entries in loop by UDP -----

#-- check nc variant. On some systems, e.g. Ubuntu the option
# -q is needed even when UDP is used, whereas on Rocky, -q causes an issue.
NC_HAS_Q=0
# Try to run nc with -q. We redirect stderr to grep.
# The grep -q command is silent and looks for common error strings.
if nc -q 0 localhost 1 2>&1 | grep -qE 'invalid|illegal|unknown option'; then
    # If grep finds an error string, it means the option is NOT supported.
    echo "INFO: This version of nc does not support the -q option."
    NC_HAS_Q=0
else
    # If grep finds no error, the option is assumed to be supported.
    echo "INFO: This version of nc supports the -q option."
    NC_HAS_Q=1
fi

echo " "
echo "#-4"
echo "----------------------------------------"
echo "--- create log msg in loop with netcat"
echo "----------------------------------------"
echo " "

# -- start syslog-ng engine -----
start_syslog

i=0
while [ "${i}" -lt "${MAX_LOOP}" ]; do
    i=$((i + 1))

    echo " "
    echo " --- LOOP ${i} ------------"
    echo " "
    LOG_MESSAGE="[INFO] This is log number ${i}: "
    LOG_MESSAGE="${LOG_MESSAGE} \"Schließt man von den Meinungen einer Epoche die intelligenten aus, bleibt die „öffentliche Meinung“\". Nicolás Gómez Dávila"
    LOG_MESSAGE="${LOG_MESSAGE} 🤪 ¿Qué pasa, señor? he price is > 100€, < 200£, or ~ \$150."
    LOG_MESSAGE="${LOG_MESSAGE} (Special chars: !@#$%^&*\`§©®™) 🌍🤪"
    echo "${LOG_MESSAGE}" >"${TEST}/plainlog_${i}.txt"
    sleep 0.25

    # nc -q 3 -u 127.0.0.1 "${UDP_PORT}" <"${TEST}/plainlog_${i}.txt"
    # on some platforms, e.g. ubuntu -q option is needed some platforms do not
    # provide option q at all.
    if [ "${NC_HAS_Q}" -eq 1 ]; then
        nc -q 3 -u 127.0.0.1 "${UDP_PORT}" <"${TEST}/plainlog_${i}.txt"
    else
        nc -u 127.0.0.1 "${UDP_PORT}" <"${TEST}/plainlog_${i}.txt"
    fi

    sleep 0.25

    echo "LOOP ${i}: nc -u 127.0.0.1 ${UDP_PORT} < ${TEST}/plainlog_${i}.txt"

    if ! [ -f "${TEST}/mac.dat" ]; then
        echo "WARNING ${TEST}/mac.dat does not exist. Wait 3 seconds.."
        sleep 3
    fi
    if ! [ -f "${TEST}/mac.dat" ]; then
        echo "ERROR! ${TEST}/mac.dat does still not exist."
        stop_syslog
        exit 1
    fi

    if ! [ -f "${TEST}/messages.slog" ]; then
        echo "WARNING: ${TEST}/messages.slog does not exist. Wait 3 seconds.."
        sleep 3
    fi
    if ! [ -f "${TEST}/messages.slog" ]; then
        echo "ERROR! ${TEST}/messages.slog does still not exist."
        stop_syslog
        exit 1
    fi

    # now that file handes are closed, save a copy of current file state for
    # each
    i_offset=$((i + counter_value))
    echo "i_offset: ${i_offset}"
    if ! [ -f "${TEST}/mac0.dat" ]; then
        echo "!!!"
        echo "!!! WARNING: mac0.dat was not existent !!!"
        echo "!!! This is not expected to happen when latest syslog-ng is used."
        echo "!!! A copy of mac1.dat will be used later instead but will cause a"
        echo "!!! verification error later"
        echo "!!!"
        cnt_error=$((cnt_error + 1))
    fi

    echo " "
    echo "ls -al ${TEST}"
    ls -al "${TEST}"

done # while

# -- stop syslog-ng engine -----
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

# -- key counter -----
echo " "
echo "#-6"
echo " "

echo "Check if files mac.dat and host.key do exist .."
if ! [ -f "${TEST}/mac.dat" ]; then
    echo "ERROR: ${TEST}/mac.dat does not exist"
    cnt_error=$((cnt_error + 1))
fi
if ! [ -f "${TEST}/host.key" ]; then
    echo "ERROR: ${TEST}/host.key does not exist"
    cnt_error=$((cnt_error + 1))
fi

echo "${BIN}/slogkey --counter ${TEST}/host.key"
"${BIN}"/slogkey --counter "${TEST}/host.key" 2>&1 | tee "${TEST}/host-key-counter-testend.log"
key_count=$(grep 'counter=' "${TEST}/host-key-counter-testend.log" | cut -d'=' -f2)
echo "Host.key counter: ${key_count}"
if [ "${key_count}" -lt 1 ]; then
    echo "invalid key_count"
    cnt_error=$((cnt_error + 1))
fi

RESULT=$((counter_value + MAX_LOOP))
echo "counter_value + MAX_LOOP: ${RESULT}"
if [ "${key_count}" -lt "${RESULT}" ]; then
    echo "invalid key_count, expected at least ${RESULT}"
    cnt_error=$((cnt_error + 1))
fi

if [ "${COPY_TO_HOME_BACKUP}" = "true" ]; then
    # copy from /tmp/test/slog/ to home folder ~/test/<scriptname>/
    cp -R "${TEST}/." "${HOME_BACKUP}/"
    # list content
    echo "#-11"
    echo "ls -al ${HOME_BACKUP}/"
    ls -al "${HOME_BACKUP}/"
fi

# cleanup /tmp/${SUBFOLDER_TEST}/
if [ "${KEEP_DATA}" = true ]; then
    echo "Data in ${TEST} will not be deleted!"
else
    ls -alt /tmp/"${SUBFOLDER_TEST}"/
    rm -rf /tmp/"${SUBFOLDER_TEST}"/"${SUBFOLDER}_${PATH_SUFFIX}"/
    ls -alt /tmp/"${SUBFOLDER_TEST}"/
fi

echo " "
echo " "
echo "#-7"
echo "----------------------------------------"
echo "--- Used binaries"
echo "----------------------------------------"
echo " "
echo "ls -alt ${BIN}/"
ls -alt "${BIN}/"
echo " "
echo "ls -alt ${SBIN}/"
ls -alt "${SBIN}/"
echo " "

sha256sum "${BIN}/slogkey"
sha256sum "${BIN}/slogverify"
sha256sum "${BIN}/slogencrypt"
sha256sum "${SBIN}/syslog-ng"
sha256sum "${SBIN}/syslog-ng-ctl"
echo " "

if [ "${COPY_TO_HOME_BACKUP}" = "true" ]; then
    #.txt files are deleted without prompting, .log files remain
    echo "You might want to call this script like:"
    echo "$0 2>&1 | tee ${HOME_BACKUP}/protocol_${SCRIPTNAME}_${NOW}.log"
    echo " "
fi

echo "#-8"
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
