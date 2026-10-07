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
# File:   cli15_multiline_all_logmodes_crypt_verify_n_tamper.sh
# Date:   2026-05-29
#
# Smoke Test of cli tools slogkey, slogencrypt and slogverify
# Needed keys are generated in test.

# set -x
set -o pipefail

VERSION="Version 1.1.2"

IS_BIN_SUPPORT_PLAIN="true"
INPUT_FILE="inputfile_multiline.txt"
# COPY_TO_HOME_BACKUP="false"
COPY_TO_HOME_BACKUP="true"
MACADDRESS="01:23:45:67:89:AB"
SERIALNUMBER="12345678"

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
BIN=${PREFIX}/bin
SBIN=${PREFIX}/sbin
SUBFOLDER_TEST="test_slog"
PATH_SUFFIX="${SCRIPTNAME}_${NOW}_${PID}_${CLEAN_ID}"
SUBFOLDER="data"
TEST=/tmp/${SUBFOLDER_TEST}/${SUBFOLDER}_${PATH_SUFFIX}
echo "TEST: ${TEST}"

HOME_BACKUP=${HOME}/${SUBFOLDER_TEST}/${SCRIPTNAME}
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
# Helper function to stop running syslog-ng background process
stop_syslog() {
    if ! "${SCRIPT_DIR}"/stop_syslog-ng.sh; then
        echo "Warning: Failed to stop syslog-ng"
    fi
}

#-----------------------------------------------------------------------
# Helper function to modifiy one byte in given file at given position
# When given byte is the same as existing, a random byte is chosen instead.
# Usage: modify_byte_v2 <file> <position> <byte>
# Example: modify_byte_v2 ./file2tamper.txt 42 10

modify_byte_v2() {
    local file_path="$1"
    local position="$2"
    local tamperbyte="$3"

    # Validation
    if [[ ! -f ${file_path} ]]; then
        echo "Error: File not found." >&2
        return 1
    fi

    # Read the current byte
    local current_byte
    current_byte=$(od -An -N1 -j "${position}" -tu1 "${file_path}" | tr -d ' ')
    if [[ -z ${current_byte} ]]; then
        echo "Error: Could not read byte at position ${position}." >&2
        return 1
    fi

    # Logic to ensure the byte changes
    local new_byte="${tamperbyte}"
    if [[ ${new_byte} -eq ${current_byte} ]]; then
        new_byte=$(((new_byte + 1) % 256))
        echo "Note: Tamperbyte matched current byte; incremented to ${new_byte}."
    fi

    # Write the byte safely
    printf "\\x$(printf '%02x' "${new_byte}")" | dd of="${file_path}" bs=1 seek="${position}" count=1 conv=notrunc status=none

    echo "Changed byte at ${position} from ${current_byte} to ${new_byte}."
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
rm -f "${TEST}"/*.out "${TEST}"/*.log "${TEST}"/*.slo* 2>/dev/null

if [[ ${COPY_TO_HOME_BACKUP} == "true" ]]; then
    rm -f "${HOME_BACKUP}"/*.key "${HOME_BACKUP}"/*.dat "${HOME_BACKUP}"/*.txt "${HOME_BACKUP}"/*.chk 2>/dev/null
    rm -f "${HOME_BACKUP}"/*.out "${HOME_BACKUP}"/*.slo* 2>/dev/null
fi

cp "${HOMESLOGTEST}/${INPUT_FILE}" "${TEST}/${INPUT_FILE}"

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

#
# -- LOOP over log modes -----
#

LOGMODES=("direct" "base64" "enc")
TOTAL_LOG_MODES=${#LOGMODES[@]}
k=0
while [[ ${k} -lt ${TOTAL_LOG_MODES} ]]; do
    # HACK only ONE MULTI-LINE-FILE, overwrite keys and macs in each loop
    i=0
    current_logmode="${LOGMODES[${k}]}"
    k=$((k + 1))

    i=$((i + 1))
    j=$((i - 1))

    echo " "
    echo "----------------------------------------"
    echo "--- slogencrypt, logmode: ${current_logmode}"
    echo "--- Loop i: ${i}, j: ${j}"
    echo "----------------------------------------"
    echo " "
    if [[ ${IS_BIN_SUPPORT_PLAIN} == "true" ]]; then
        "${BIN}/slogencrypt" \
            --key-file "${TEST}/h${j}.key" \
            --mac-file "${TEST}/hmac${j}.dat" \
            --logmode "${current_logmode}" \
            "${TEST}/h${i}.key" \
            "${TEST}/hmac${i}.dat" \
            "${TEST}/${INPUT_FILE}" \
            "${TEST}/${INPUT_FILE}_${j}.slog"
        ret=${PIPESTATUS[0]}
        if ((ret != 0)); then
            cnt_error=$((cnt_error + 1))
            echo "slogencrypt failed (rc=${ret})" >&2
        fi
    else
        "${BIN}/slogencrypt" \
            --key-file "${TEST}/h${j}.key" \
            --mac-file "${TEST}/hmac${j}.dat" \
            "${TEST}/h${i}.key" \
            "${TEST}/hmac${i}.dat" \
            "${TEST}/${INPUT_FILE}" \
            "${TEST}/${INPUT_FILE}_${j}.slog"
        ret=${PIPESTATUS[0]}
        if ((ret != 0)); then
            cnt_error=$((cnt_error + 1))
            echo "slogencrypt failed (rc=${ret})" >&2
        fi
    fi

    echo " "
    sleep 1

    check_if_not_exit "${TEST}/h${j}.key"
    check_if_not_exit "${TEST}/hmac${j}.dat"
    check_if_not_exit "${TEST}/h${i}.key"
    check_if_not_exit "${TEST}/hmac${i}.dat"
    check_if_not_exit "${TEST}/${INPUT_FILE}"
    check_if_not_exit "${TEST}/${INPUT_FILE}_${j}.slog"

    echo "${BIN}/slogkey --counter ${TEST}/h${j}.key"
    "${BIN}"/slogkey --counter "${TEST}/h${j}.key"

    echo "${BIN}/slogkey --counter ${TEST}/h${i}.key"
    "${BIN}"/slogkey --counter "${TEST}/h${i}.key"

    # -- TAMPERING ---
    echo "-- TAMPERING ---"
    echo "cp  ${TEST}/${INPUT_FILE}_${j}.slog ${TEST}/${INPUT_FILE}_err_${j}.slog"
    cp "${TEST}/${INPUT_FILE}_${j}.slog" "${TEST}/${INPUT_FILE}_err_${j}.slog"
    echo "modify_byte_v2 ${TEST}/${INPUT_FILE}_err_${j}.slog 42 10"
    modify_byte_v2 "${TEST}/${INPUT_FILE}_err_${j}.slog" 42 10
    ret=${PIPESTATUS[0]}
    if ((ret != 0)); then
        cnt_error=$((cnt_error + 1))
        echo "Failed to manipulate log file, so the verification with a manipulated log file is not possible! (rc=${ret})" >&2
    fi

    echo " "
    echo "----------------------------------------"
    echo "--- slogverify NORMAL MODE of tampered slog, logmode: ${current_logmode}"
    echo "--- Loop i: ${i}, j: ${j}"
    echo "----------------------------------------"
    echo " "
    check_if_not_exit "${TEST}/h0.key"
    check_if_not_exit "${TEST}/hmac${i}.dat"
    if [[ ${IS_BIN_SUPPORT_PLAIN} == "true" ]]; then
        "${BIN}/slogverify" \
            --key-file "${TEST}/h0.key" \
            --mac-file "${TEST}/hmac${i}.dat" \
            --logmode "${current_logmode}" \
            "${TEST}/${INPUT_FILE}_err_${j}.slog" \
            "${TEST}/${INPUT_FILE}.slog_verified_err_${j}.txt" 2>&1 | tee "${TEST}/slogverify-normal-mode-result_err_${j}.log"
        ret=${PIPESTATUS[0]}
        if ((ret == 0)); then
            # MUST fail when no error is returned!
            cnt_error=$((cnt_error + 1))
            echo "slogverify failed (rc=${ret})" >&2
        fi
    else
        "${BIN}/slogverify" \
            --key-file "${TEST}/h0.key" \
            --mac-file "${TEST}/hmac${i}.dat" \
            "${TEST}/${INPUT_FILE}_err_${j}.slog" \
            "${TEST}/${INPUT_FILE}.slog_verified_err_${j}.txt" 2>&1 | tee "${TEST}/slogverify-normal-mode-result_err_${j}.log"
        ret=${PIPESTATUS[0]}
        if ((ret == 0)); then
            # MUST fail when no error is returned!
            cnt_error=$((cnt_error + 1))
            echo "slogverify failed (rc=${ret})" >&2
        fi
    fi

    check_and_cat "${TEST}/${INPUT_FILE}_err_${j}.slog"
    check_and_cat "${TEST}/${INPUT_FILE}.slog_verified_err_${j}.txt"
    check_and_cat "${TEST}/slogverify-normal-mode-result_err_${j}.log"

    if ! [[ -f "${TEST}/slogverify-normal-mode-result_err_${j}.log" ]]; then
        printf "ERROR: %s does not exist\n" "${TEST}/slogverify-normal-mode-result_err_${j}.log" >&2
        cnt_error=$((cnt_error + 1))
    else
        if grep -Fq "[SLOG] ERROR" "${TEST}/slogverify-normal-mode-result_err_${j}.log"; then
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

done

# check whether mac.dat has been accessed - it should not because syslog-ng not
# running!
# If it has been provided by slogencrypt, content should be the same as
# mac0.dat.
echo " "

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
