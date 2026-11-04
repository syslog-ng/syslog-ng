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
# File:   cli14_plain_loggen_file_verify_n.sh
# Date:   2026-05-29
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

# set -x
set -o pipefail

VERSION="Version 1.4.3"

IS_BIN_SUPPORT_PLAIN="true"
# Log mode is one of (direct | base64 | enc)
LOGMODE="direct"

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

SCRIPT_DIR="$(cd -- "$(dirname -- "$0")" && pwd -P)"
echo "SCRIPT_DIR: ${SCRIPT_DIR}"

PATH_PREFIX_VALUE=$("${SCRIPT_DIR}"/get_prefix.sh)
# This path must fit the one given to the build system where binaries are provided.
PREFIX=${PATH_PREFIX_VALUE}
echo "PREFIX: ${PREFIX}"
if [[ ! -e ${PREFIX} ]]; then
    echo "ERROR: Required path PREFIX for installed binaries not found."
    echo "Maybe the project has not yet been built or the build directory was moved or deleted."
    echo "The path to slogkey, slogencrypt and slogverify is PREFIX/bin"
    echo "You can try to set PREFIX manually here in the script instead of using get_prefix.sh"
    echo "Example: PREFIX=${HOME}/Software/install"
    echo "FAIL"
    exit 1
fi

HOMESLOGTEST=${SCRIPT_DIR}
UDP_PORT=7777

# -- syslog-ng.conf ---
# The default path for test where the syslog-ng configuration is copied into is:
#    "/tmp/test_slog/data.."
# The prepared conf file, e.g. syslog-ng-test-udp-nc.conf, will be renamed into syslog-ng.conf.
# In the source code test folder, prepared conf files are provoided and are located here:
#    modules/secure-logging/tests/

SFNCONF="syslog-ng-test-udp-nc.conf"
if [[ ${IS_BIN_SUPPORT_PLAIN} == "true" && ${LOGMODE} == "direct" ]]; then
    SFNCONF="syslog-ng-test-plain-direct-udp-nc.conf"
elif [[ ${IS_BIN_SUPPORT_PLAIN} == "true" && ${LOGMODE} == "base64" ]]; then
    SFNCONF="syslog-ng-test-plain-base64-udp-nc.conf"
elif [[ ${IS_BIN_SUPPORT_PLAIN} == "true" && ${LOGMODE} == "enc" ]]; then
    SFNCONF="syslog-ng-test-udp-nc.conf"
else
    echo "syslog-ng is used in encrypted mode, old conf template."
    SFNCONF="syslog-ng-test-udp-nc_old.conf"
fi
echo "LOGMODE: ${LOGMODE}, syslog-ng.conf <-- ${SFNCONF}"

BIN=${PREFIX}/bin
SBIN=${PREFIX}/sbin
ETC=${PREFIX}/etc
VAR=${PREFIX}/var

SUBFOLDER_TEST="test_slog"
PATH_SUFFIX="${SCRIPTNAME}_${NOW}_${PID}_${CLEAN_ID}"
SUBFOLDER="data"
TEST=/tmp/${SUBFOLDER_TEST}/${SUBFOLDER}_${PATH_SUFFIX}
echo "TEST: ${TEST}"

HOME_BACKUP=${HOME}/${SUBFOLDER_TEST}/${SCRIPTNAME}
# COPY_TO_HOME_BACKUP="false"
COPY_TO_HOME_BACKUP="true"
MACADDRESS="01:23:45:67:89:AB"
SERIALNUMBER="12345678"
PAYLOAD=payload_utf8_2048.txt
PAYLOAD_FILE=${TEST}/${PAYLOAD}

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

    echo "IS_BIN_SUPPORT_PLAIN: ${IS_BIN_SUPPORT_PLAIN}"
    if [[ ${IS_BIN_SUPPORT_PLAIN} == "true" ]]; then
        echo "LOGMODE: ${LOGMODE}"
    fi

    echo "BIN: ${BIN}"
    echo "SBIN: ${SBIN}"
    echo "ETC: ${ETC}"
    echo "VAR: ${VAR}"
    echo "TEST: ${TEST}"
    echo "SFNCONF: ${SFNCONF}"
    echo "UDP_PORT: ${UDP_PORT}"
    echo "PAYLOAD: ${PAYLOAD}"

    echo "COPY_TO_HOME_BACKUP: ${COPY_TO_HOME_BACKUP}"
    if [[ ${COPY_TO_HOME_BACKUP} == "true" ]]; then
        echo "HOME_BACKUP: ${HOME_BACKUP}"
    fi

    # stop working. exit now.
    exit 1
}

#-----------------------------------------------------------------------
# -- helper function to check of all files in given array do exist
# Usage: check_missing "path1" "path2" "path3" ...
check_missing() {
    for path in "$@"; do
        # Check if the path does NOT exist (-e works for files and directories)
        if [[ ! -e ${path} ]]; then
            echo "Error: Required path '${path}' not found."
            check_script_config
            # ERROR, check_script_config will exit 1
        fi
    done
    return 0 # SUCESS, all files found
}

#-----------------------------------------------------------------------
# Helper function to stop running syslog-ng background process
stop_syslog() {
    if ! "${SCRIPT_DIR}"/stop_syslog-ng.sh; then
        echo "Warning: Failed to stop syslog-ng"
    fi
}

#-----------------------------------------------------------------------
# Helper function to start syslog-ng background processi
# returns 0 when syslog-ng has been started successfull else 1
start_syslog() {
    if ! "${SCRIPT_DIR}"/start_syslog-ng.sh "${PATH_SUFFIX}"; then
        echo "ERROR: start_syslog: Failed to start syslog-ng"
        return 1
    fi
    # syslog-ng started successfully
    return 0
}

echo " "
"${SCRIPT_DIR}"/get_git_info.sh

mkdir -p "${TEST}"
if [[ ${COPY_TO_HOME_BACKUP} == "true" ]]; then
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
rm -f "${TEST}"/*.key "${TEST}"/*.dat "${TEST}"/*.txt "${TEST}"/*.chk 2>/dev/null
rm -f "${TEST}"/*.out "${TEST}"/*.log "${TEST}"/*.slo* 2>/dev/null

if [[ ${COPY_TO_HOME_BACKUP} == "true" ]]; then
    rm -f "${HOME_BACKUP}"/*.key "${HOME_BACKUP}"/*.dat "${HOME_BACKUP}"/*.txt "${HOME_BACKUP}"/*.chk 2>/dev/null
    rm -f "${HOME_BACKUP}"/*.out "${HOME_BACKUP}"/*.slo* 2>/dev/null
fi

if [[ -f "${HOMESLOGTEST}/${PAYLOAD}" ]]; then
    if ! [[ -f ${PAYLOAD_FILE} ]]; then
        # Only ever do copy the example payload file, when user
        # hasn't provided a payload file
        echo "cp ${HOMESLOGTEST}/${PAYLOAD} ${PAYLOAD_FILE}"
        cp "${HOMESLOGTEST}/${PAYLOAD}" "${PAYLOAD_FILE}"
    fi
fi

if ! [[ -f ${PAYLOAD_FILE} ]]; then
    echo "ERROR! File PAYLOAD_FILE Not found: ${PAYLOAD_FILE}"
    echo " "
    echo " Note: PAYLOAD_FILE must conform RFC5424"
    echo " Example:"
    echo " <38>1 2025-07-08T11:27:24+02:00 localhost prg00000 1234 ID01 - seq: 0000000040, thread: 101, runid: 42153, stamp: 2025-07-08T11:27:24 Sadly, however, before she could get to a phone to tell anyone about it, a terrible, stupid catastrophe occurred, and the idea was lost for ever."
    echo " <38>1 2025-07-08T11:27:24+02:00 localhost prg00000 1234 ID02 - seq: 0000000041, thread: 101, runid: 42153, stamp: 2025-07-08T11:27:24 This is not her story."
    echo " <38>1 2025-07-08T11:27:24+02:00 localhost prg00000 1234 ID03 - seq: 0000000042, thread: 101, runid: 42153, stamp: 2025-07-08T11:27:24 But it is the story of that terrible, stupid catastrophe and some of its consequences."
    echo " "
    check_script_config
fi

# config working path in syslog-ng.conf

cp -f "${HOMESLOGTEST}/${SFNCONF}" "${TEST}/syslog-ng.conf"
check_missing "${TEST}/syslog-ng.conf" "${SCRIPT_DIR}/update_conf_path.sh"
echo "Update syslog-ng.conf template path .."
RETVALUC=$("${SCRIPT_DIR}/update_conf_path.sh" "${TEST}/syslog-ng.conf" "${PATH_SUFFIX}" "add")
echo "RETVALUC: ${RETVALUC}"

# Generate Key or check existing

if ! [[ -f "${TEST}/host.key" ]]; then
    if ! [[ -f "${TEST}/master.key" ]]; then
        # create master.key needed to create host.key
        echo " "
        echo "#-2 Create master key"
        "${BIN}/slogkey" \
            -m "${TEST}/master.key"
        if ! [[ -f "${TEST}/master.key" ]]; then
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
if ! [[ -f "${TEST}/h0.key" ]]; then
    echo "ERROR! Not found: ${TEST}/h0.key"
    check_script_config
fi
if ! [[ -f "${TEST}/host.key" ]]; then
    echo "ERROR! Not found: ${TEST}/host.key"
    check_script_config
fi

check_missing "${TEST}/h0.key" "${TEST}/host.key" "${HOMESLOGTEST}/${SFNCONF}"

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
echo "----------------------------------------"
echo "--- create log msg with loggen via netcat"
echo "----------------------------------------"
echo " "

# -- start syslog-ng engine -----
#
if ! start_syslog; then
    echo "FAIL"
    exit 1
fi

echo " "
echo "cat ${PAYLOAD_FILE}"
cat "${PAYLOAD_FILE}"

echo " "
# TODO clarify: How to avoid the following ERRORS / WARNINGS
# RFC5425 style octet count was found at the start of the message, this is probably not what was intended; data='.[.,..▒',
# error [socket_plugin.c:active_thread_func:384] can't generate more log lines. end of input file?

echo "${BIN}/loggen -i -D -P 127.0.0.1 7777 --read-file ${PAYLOAD_FILE}"
loggen_output=$("${BIN}/loggen" -i -D -P "127.0.0.1" "7777" --read-file "${PAYLOAD_FILE}" 2>&1)

# if [[ $loggen_output == *"Invalid line "* ]]; then
#    echo "ERROR in file PAYLOAD_FILE (${PAYLOAD_FILE} or ${HOMESLOGTEST}/${PAYLOAD}):  invalid line was detected."
#    cnt_error=$((cnt_error + 1))
# else
#    echo "Tool loggen executed successfully. No invalid lines found."
# fi

case "${loggen_output}" in
*"Invalid line "*)
    echo "ERROR in file PAYLOAD_FILE (${PAYLOAD_FILE} or ${HOMESLOGTEST}/${PAYLOAD}):  invalid line was detected."
    cnt_error=$((cnt_error + 1))
    ;;
*)
    echo "Tool loggen executed successfully. No invalid lines found."
    ;;
esac

# -- stop syslog-ng engine -----
stop_syslog

echo " "
echo " "
echo "----------------------------------------"
echo "--- slogverify NORMAL MODE"
echo "----------------------------------------"
echo " "

if [[ ${IS_BIN_SUPPORT_PLAIN} == "true" ]]; then
    "${BIN}/slogverify" \
        --key-file "${TEST}/h0.key" \
        --mac-file "${TEST}/mac.dat" \
        --logmode "${LOGMODE}" \
        "${TEST}/messages.slog" \
        "${TEST}/messages_verified.txt" 2>&1 | tee "${TEST}/slogverify-normal-mode-result.log"
    ret=${PIPESTATUS[0]}
    if ((ret != 0)); then
        cnt_error=$((cnt_error + 1))
        echo "slogverify failed (rc=${ret})" >&2
    fi
else
    "${BIN}/slogverify" \
        --key-file "${TEST}/h0.key" \
        --mac-file "${TEST}/mac.dat" \
        "${TEST}/messages.slog" \
        "${TEST}/messages_verified.txt" 2>&1 | tee "${TEST}/slogverify-normal-mode-result.log"
    ret=${PIPESTATUS[0]}
    if ((ret != 0)); then
        cnt_error=$((cnt_error + 1))
        echo "slogverify failed (rc=${ret})" >&2
    fi
fi

# check if output files do exist
echo " "
if ! [[ -f "${TEST}/messages.slog" ]]; then
    echo "ERROR: ${TEST}/messages.slog does not exist"
    cnt_error=$((cnt_error + 1))
else
    cat "${TEST}/messages.slog"
    echo " "
fi

if ! [[ -f "${TEST}/messages_verified.txt" ]]; then
    echo "ERROR: ${TEST}/messages_verified.txt does not exist"
    cnt_error=$((cnt_error + 1))
else
    cat "${TEST}/messages_verified.txt"
    echo " "
fi

# Log claims to be past entry from past archive. We cannot rewind back to this key without key0. This is going to fail.; entry='2'
# Decryption not successful; entry='2'
# Unable to recover; entry='3'
# Aggregated MAC mismatch. Log might be incomplete;
# There is a problem with log verification. Please check log manually;

if ! [[ -f "${TEST}/slogverify-normal-mode-result.log" ]]; then
    printf "ERROR: %s does not exist\n" "${TEST}/slogverify-normal-mode-result.log" >&2
    cnt_error=$((cnt_error + 1))
else
    if grep -q "Log claims to be past entry from past archive" "${TEST}/slogverify-normal-mode-result.log"; then
        cnt_error=$((cnt_error + 1))
    fi
    if grep -q "Decryption not successful" "${TEST}/slogverify-normal-mode-result.log"; then
        cnt_error=$((cnt_error + 1))
    fi
    # only when fail and logmode direct | base64:
    if grep -q "Verification not successful" "${TEST}/slogverify-normal-mode-result.log"; then
        cnt_error=$((cnt_error + 1))
    fi
    if grep -q "Unable to recover" "${TEST}/slogverify-normal-mode-result.log"; then
        cnt_error=$((cnt_error + 1))
    fi
    if grep -q "Aggregated MAC mismatch" "${TEST}/slogverify-normal-mode-result.log"; then
        cnt_error=$((cnt_error + 1))
    fi
    if grep -q "There is a problem with log verification. Please check log manually" "${TEST}/slogverify-normal-mode-result.log"; then
        cnt_error=$((cnt_error + 1))
    fi
    if grep -Fq "[SLOG] ERROR" "${TEST}/slogverify-normal-mode-result.log"; then
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
echo "${BIN}/slogkey --counter ${TEST}/h0.key"
"${BIN}/slogkey" --counter "${TEST}/h0.key"

echo " "
echo "${BIN}/slogkey --counter ${TEST}/host.key"
"${BIN}/slogkey" --counter "${TEST}/host.key"

if [[ ${COPY_TO_HOME_BACKUP} == "true" ]]; then
    cp -R "${TEST}/." "${HOME_BACKUP}/"
fi

echo " "
echo "Check if files mac0.dat, mac.dat, h0.key and host.key do exist .."
if ! [[ -f "${TEST}/mac0.dat" ]]; then
    echo "ERROR: ${TEST}/mac0.dat does not exist"
    cnt_error=$((cnt_error + 1))
fi
if ! [[ -f "${TEST}/mac.dat" ]]; then
    echo "ERROR: ${TEST}/mac.dat does not exist"
    cnt_error=$((cnt_error + 1))
fi

if ! [[ -f "${TEST}/h0.key" ]]; then
    echo "ERROR: ${TEST}/h0.key does not exist"
    cnt_error=$((cnt_error + 1))
fi
if ! [[ -f "${TEST}/host.key" ]]; then
    echo "ERROR: ${TEST}/host.key does not exist"
    cnt_error=$((cnt_error + 1))
fi

# cleanup /tmp/${SUBFOLDER_TEST}/
rm -rf /tmp/"${SUBFOLDER_TEST}"/"${SUBFOLDER}_${PATH_SUFFIX}"/

echo " "
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
sha256sum "${BIN}/loggen"
sha256sum "${SBIN}/syslog-ng"
sha256sum "${SBIN}/syslog-ng-ctl"
echo " "

echo "You might want to call this script like:"
echo "$0 2>&1 | tee ./protocol_${SCRIPTNAME}_${NOW}.log"
if [[ ${COPY_TO_HOME_BACKUP} == "true" ]]; then
    echo "$0 2>&1 | tee ${HOME_BACKUP}/protocol_${SCRIPTNAME}_${NOW}.log"
    echo " "
fi
echo " "

echo "return cnt_error: ${cnt_error}"
#if ((cnt_error == 0)); then
if [[ ${cnt_error} -eq 0 ]]; then
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
