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
# File:   cli12v4_direct_msg_crypt_verify_i.sh
# Date:   2026-05-29
#
# Smoke Test of cli tools slogkey, slogencrypt and slogverify
# Needed keys are generated in test.

# set -x
set -o pipefail

VERSION="Version 1.4.5"

IS_BIN_SUPPORT_PLAIN="true"
# Log mode is one of (direct | base64 | enc)
# NOTE: This script only works for direct
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

MAX_LOOP=25
BIN=${PREFIX}/bin
SBIN=${PREFIX}/sbin

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
    echo "TEST: ${TEST}"

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
# -- helper function to check whether single file exists and call cat
#    when it exists. In case it does not exists, global error counter
#    is incremented.
# Retuns 0 when success else 1 in case of error
# Usage: check_and_cat path
check_and_cat() {
    local path_check="$1"
    if ! [[ -f ${path_check} ]]; then
        echo "ERROR: ${path_check} does not exist"
        cnt_error=$((cnt_error + 1))
        return 1
    else
        echo " "
        echo "cat ${path_check}"
        cat "${path_check}"
        echo " "
    fi
    return 0
}

#-----------------------------------------------------------------------
# -- helper function to check whether single file exists and calls exit
#    in case the file does not exist. Exit because the file might be a
#    precondition for next step in the script.
# Retuns 0 when success else exit
# Usage: check_if_not_exit path

check_if_not_exit() {
    local path_check="$1"
    if ! [[ -f ${path_check} ]]; then
        echo "ERROR: ${path_check} does not exist"
        cnt_error=$((cnt_error + 1))
        echo "FAIL"
        exit 1
    fi
    echo "exists: ${path_check}"
    return 0
}

#-----------------------------------------------------------------------
# -- helper function to check output file by tee of verification of iterative
#    mode.
#    Usage: check_verify_iterative "path_slog" "path_slog_verified" "path_tee"

check_verify_iterative() {
    echo " "
    echo "----------------------------------------"
    echo "--- check_verify_iterative"
    echo "----------------------------------------"
    echo " "

    # This full file name is the output of slogencrypt
    # e.g.: "${TEST}/${INPUT_FILE}.slog"
    local path_slog="$1"

    # This full file name is the output of slogverify
    # e.g.: "${TEST}/${INPUT_FILE}.chk"
    local path_slog_verified="$2"

    # This full file name is the console log of slogverify
    # captured by tee
    # e.g.: "${TEST}/slogverify-iterative-mode-result.log"
    local path_tee="$3"

    # check if slogencrypt output file existsi
    if ! check_and_cat "${path_slog}"; then
        cnt_error=$((cnt_error + 1))
        return 1
    fi

    # check if slogverify output file exists
    if ! check_and_cat "${path_slog_verified}"; then
        cnt_error=$((cnt_error + 1))
        return 1
    fi

    # check if slogverify console log file captured with tee exists
    if ! check_and_cat "${path_tee}"; then
        cnt_error=$((cnt_error + 1))
        return 1
    fi

    if grep -q "Log claims to be past entry from past archive" "${path_tee}"; then
        cnt_error=$((cnt_error + 1))
        return 1
    fi
    if grep -q "Decryption not successful" "${path_tee}"; then
        cnt_error=$((cnt_error + 1))
        return 1
    fi
    # only when fail and logmode direct | base64:
    if grep -q "Verification not successful" "${path_tee}"; then
        cnt_error=$((cnt_error + 1))
        return 1
    fi
    if grep -q "Unable to recover" "${path_tee}"; then
        cnt_error=$((cnt_error + 1))
        return 1
    fi
    if grep -q "Aggregated MAC mismatch" "${path_tee}"; then
        cnt_error=$((cnt_error + 1))
        return 1
    fi
    if grep -q "There is a problem with log verification." "${path_tee}"; then
        cnt_error=$((cnt_error + 1))
        return 1
    fi
    if grep -Fq "[SLOG] ERROR" "${path_tee}"; then
        cnt_error=$((cnt_error + 1))
        return 1
    fi

    return 0
}

#-----------------------------------------------------------------------
# Helper function to stop running syslog-ng background process
stop_syslog() {
    if ! "${SCRIPT_DIR}"/stop_syslog-ng.sh; then
        echo "Warning: Failed to stop syslog-ng"
    fi
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

check_missing "${HOMESLOGTEST}" "${SBIN}/syslog-ng" "${SBIN}/syslog-ng-ctl" \
    "${BIN}/slogencrypt" "${BIN}/slogverify" "${BIN}/slogkey"

# cleanup files from previous tests
rm -f "${TEST}"/*.key "${TEST}"/*.dat "${TEST}"/*.txt "${TEST}"/*.chk 2>/dev/null
rm -f "${TEST}"/*.slog "${TEST}"/*.log "${TEST}"/*.slo* 2>/dev/null

if [[ ${COPY_TO_HOME_BACKUP} == "true" ]]; then
    rm -f "${HOME_BACKUP}"/*.key "${HOME_BACKUP}"/*.dat "${HOME_BACKUP}"/*.txt "${HOME_BACKUP}"/*.chk 2>/dev/null
    rm -f "${HOME_BACKUP}"/*.slog "${HOME_BACKUP}"/*.slo* 2>/dev/null
fi

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

check_missing "${TEST}/h0.key" "${TEST}/host.key"

# -- stop syslog-ng engine -----
stop_syslog

# -- create log entries by slogencrypt -----
echo "----------------------------------------"
echo "--- create text files used later as log entry"
echo "----------------------------------------"
echo " "
i=0
while [[ ${i} -lt ${MAX_LOOP} ]]; do
    i=$((i + 1))
    echo "i: ${i} of ${MAX_LOOP}"
    LOG_MESSAGE="Log msg ${i} 🚀🌍🔭 (Special chars: < >!@#€£,%^&*\`§©®™) 🤪 0123456789"
    echo "${LOG_MESSAGE}" >"${TEST}/plainlog_${i}.txt"
    sleep 0.25
done

# Example from man page:
#  slogverify --iterative --prev-key-file host.key.2 --prev-mac-file mac.dat.2 --mac-file mac.dat
#       /var/log/messages.slog.3 /var/log/verified/messages.3

# Note:
# Old version of slogverify does NOT provide an initial MAC file.
# DO NOT USE `touch "${ETC}/mac0.dat"` to create an initial mac dat.
#
# slogencrypt has been fixed now, so that it is working without an existing mac file.
# When testing old versions of slogencrypt, mac1.dat is copied as mac0.dat.
# If this happens, slogverify is not working correctly for the first chunk using mac0.dat.

i=0
while [[ ${i} -lt ${MAX_LOOP} ]]; do
    i=$((i + 1))
    j=$((i - 1))
    echo " "
    OUT="-- Loop i: ${i}, j: ${j} ---"
    echo "${OUT}"
    echo "---- Encrypting file"

    if [[ ${IS_BIN_SUPPORT_PLAIN} == "true" ]]; then
        "${BIN}/slogencrypt" \
            --key-file "${TEST}/h${j}.key" \
            --mac-file "${TEST}/mac${j}.dat" \
            --logmode "${LOGMODE}" \
            "${TEST}/h${i}.key" \
            "${TEST}/mac${i}.dat" \
            "${TEST}/plainlog_${i}.txt" \
            "${TEST}/plainlog_${i}.slog"
        ret=${PIPESTATUS[0]}
        if ((ret != 0)); then
            cnt_error=$((cnt_error + 1))
            echo "slogencrypt failed (rc=${ret})" >&2
        fi
    else
        "${BIN}/slogencrypt" \
            --key-file "${TEST}/h${j}.key" \
            --mac-file "${TEST}/mac${j}.dat" \
            "${TEST}/h${i}.key" \
            "${TEST}/mac${i}.dat" \
            "${TEST}/plainlog_${i}.txt" \
            "${TEST}/plainlog_${i}.slog"
        ret=${PIPESTATUS[0]}
        if ((ret != 0)); then
            cnt_error=$((cnt_error + 1))
            echo "slogencrypt failed (rc=${ret})" >&2
        fi
    fi

    check_if_not_exit "${TEST}/h${i}.key"
    check_if_not_exit "${TEST}/mac${i}.dat"
    check_if_not_exit "${TEST}/plainlog_${i}.txt"
    check_if_not_exit "${TEST}/plainlog_${i}.slog"

    if ! check_and_cat "${TEST}/plainlog_${i}.txt"; then
        cnt_error=$((cnt_error + 1))
    fi

    if ! check_and_cat "${TEST}/plainlog_${i}.slog"; then
        cnt_error=$((cnt_error + 1))
    fi

    echo " "
    echo "---- Iterative verification"
    echo " "
    echo " "

    if [[ ${IS_BIN_SUPPORT_PLAIN} == "true" ]]; then
        "${BIN}/slogverify" -i \
            --prev-key-file "${TEST}/h${j}.key" \
            --prev-mac-file "${TEST}/mac${j}.dat" \
            --mac-file "${TEST}/mac${i}.dat" \
            --logmode "${LOGMODE}" \
            "${TEST}/plainlog_${i}.slog" \
            "${TEST}/plainlog_${i}.chk" 2>&1 | tee "${TEST}/slogverify-iterative-mode-result_${i}.log"
        ret=${PIPESTATUS[0]}
        if ((ret != 0)); then
            cnt_error=$((cnt_error + 1))
            echo "slogverify failed (rc=${ret})" >&2
        fi
    else
        "${BIN}/slogverify" -i \
            --prev-key-file "${TEST}/h${j}.key" \
            --prev-mac-file "${TEST}/mac${j}.dat" \
            --mac-file "${TEST}/mac${i}.dat" \
            "${TEST}/plainlog_${i}.slog" \
            "${TEST}/plainlog_${i}.chk" 2>&1 | tee "${TEST}/slogverify-iterative-mode-result_${i}.log"
        ret=${PIPESTATUS[0]}
        if ((ret != 0)); then
            cnt_error=$((cnt_error + 1))
            echo "slogverify failed (rc=${ret})" >&2
        fi
    fi

    check_verify_iterative \
        "${TEST}/plainlog_${i}.slog" \
        "${TEST}/plainlog_${i}.chk" \
        "${TEST}/slogverify-iterative-mode-result_${i}.log"

done

echo " "
echo "----------------------------------------"
echo "--- Check generated keys, log, mac"
echo "----------------------------------------"
echo " "

# -- key counter -----
i=0
ALL=$((MAX_LOOP + 1))
while [[ ${i} -lt ${ALL} ]]; do
    echo " "
    echo "${BIN}/slogkey --counter ${TEST}/h${i}.key"
    "${BIN}"/slogkey --counter "${TEST}/h${i}.key"
    i=$((i + 1))
done

# mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
# Now, do test whether manipulation of a log file is detected.

if [[ ${cnt_error} -eq 0 ]]; then
    # do the following only when no errors so far

    i=0
    while [[ ${i} -lt ${MAX_LOOP} ]]; do
        i=$((i + 1))
        j=$((i - 1))
        echo " "
        OUT="-- Loop i: ${i}, j: ${j} ---"
        echo "${OUT}"

        echo " "
        echo " "
        echo "---- Iterative verification with tampered slog"
        echo " "
        echo " "

        # manipulate file so that verify returns with error

        echo "cp  ${TEST}/plainlog_${i}.slog ${TEST}/plainlog_err_${i}.slog"
        cp "${TEST}/plainlog_${i}.slog" "${TEST}/plainlog_err_${i}.slog"
        check_and_cat "${TEST}/plainlog_err_${i}.slog"

        # Precondition 1: Input line is a vaild log line not tampered yet
        # Precondition 2: LogMode direct
        if [[ ${LOGMODE} != "direct" ]]; then
            echo "ERROR: this variant is for LOGMODE direct only!"
            cnt_error=$((cnt_error + 1))
        fi

        # Read line
        FIRST_LINE=$(head -n 1 "${TEST}/plainlog_err_${i}.slog")
        echo "FIRST_LINE: ${FIRST_LINE}"

        LINE_COL="${FIRST_LINE:0:13}"
        echo "LINE_COL: ${LINE_COL}"

        # Extract iv-tag-Base64 String (Note: This only works in direct logmode)
        IV_TAG_BASE64="${FIRST_LINE:13:40}"
        echo "IV_TAG_BASE64: ${IV_TAG_BASE64}"

        MSG_DIRECT="${FIRST_LINE:53}"
        echo "MSG_DIRECT: ${MSG_DIRECT}"

        MSG_DIRECT_TAMPERED="${MSG_DIRECT} Flightlevel 42"
        echo "MSG_DIRECT_TAMPERED: ${MSG_DIRECT_TAMPERED}"

        NEW_TAMPERED="${LINE_COL}${IV_TAG_BASE64}${MSG_DIRECT_TAMPERED}"
        echo "NEW_TAMPERED: ${NEW_TAMPERED}"

        # Write back String
        # Use 'tail' to get everything EXCEPT the first line
        # Create a new file with the NEW_LINE, followed by the rest of the original content
        TEMP_FILE="${TEST}/temp_tf_${NOW}"
        echo "TEMP_FILE: ${TEMP_FILE}"
        echo "${NEW_TAMPERED}" >"${TEMP_FILE}"
        tail -n +2 "${TEST}/plainlog_err_${i}.slog" >>"${TEMP_FILE}"
        # Replace the original file with the new one
        mv "${TEMP_FILE}" "${TEST}/plainlog_err_${i}.slog"

        check_and_cat "${TEST}/plainlog_err_${i}.slog"

        # now do verify with tampered file

        if [[ ${IS_BIN_SUPPORT_PLAIN} == "true" ]]; then
            "${BIN}/slogverify" -i \
                --prev-key-file "${TEST}/h${j}.key" \
                --prev-mac-file "${TEST}/mac${j}.dat" \
                --mac-file "${TEST}/mac${i}.dat" \
                --logmode "${LOGMODE}" \
                "${TEST}/plainlog_err_${i}.slog" \
                "${TEST}/plainlog_err_${i}.chk" 2>&1 | tee "${TEST}/slogverify-iterative-mode-result_err_${i}.log"
            ret=${PIPESTATUS[0]}
            if ((ret == 0)); then
                # MUST fail when no error is returned!
                cnt_error=$((cnt_error + 1))
                echo "slogverify failed (rc=${ret})" >&2
            fi
        else
            "${BIN}/slogverify" -i \
                --prev-key-file "${TEST}/h${j}.key" \
                --prev-mac-file "${TEST}/mac${j}.dat" \
                --mac-file "${TEST}/mac${i}.dat" \
                "${TEST}/plainlog_err_${i}.slog" \
                "${TEST}/plainlog_err_${i}.chk" 2>&1 | tee "${TEST}/slogverify-iterative-mode-result_err_${i}.log"
            ret=${PIPESTATUS[0]}
            if ((ret == 0)); then
                # MUST fail when no error is returned!
                cnt_error=$((cnt_error + 1))
                echo "slogverify failed (rc=${ret})" >&2
            fi
        fi

        check_and_cat "${TEST}/plainlog_err_${i}.slog"
        check_and_cat "${TEST}/plainlog_err_${i}.chk"
        check_and_cat "${TEST}/slogverify-iterative-mode-result_err_${i}.log"

        if ! [[ -f "${TEST}/slogverify-iterative-mode-result_err_${i}.log" ]]; then
            printf "ERROR: %s does not exist\n" "${TEST}/slogverify-iterative-mode-result_err_${i}.log" >&2
            cnt_error=$((cnt_error + 1))
        else
            if grep -Fq "[SLOG] ERROR" "${TEST}/slogverify-iterative-mode-result_err_${i}.log"; then
                echo " "
                echo "expected error found!"
                echo " "
            else
                echo " "
                echo "expected error NOT found!"
                echo " "
                cnt_error=$((cnt_error + 1))
            fi
        fi

        # end while
    done

# end if if when no error so far
fi

if [[ ${COPY_TO_HOME_BACKUP} == "true" ]]; then
    cp -R "${TEST}/." "${HOME_BACKUP}/"
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
