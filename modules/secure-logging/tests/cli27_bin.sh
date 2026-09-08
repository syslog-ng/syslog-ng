#!/usr/bin/env bash
#############################################################################
# Copyright (c) 2026 Airbus Commercial Aircraft
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
# File:   cli27_bin.sh
# Date:   2026-06-03
#
# Helper script to call ldd for build binaries
#
# Preconditions:
#   Build done successfully and PREFIX points to binary folder
#
# Usage:
#   ./cli27_bin.sh
#
#-----------------------------------------------------------------------

# set -x
set -o pipefail

VERSION="Version 1.0.3"

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

SCRIPT_DIR="$(cd -- "$(dirname -- "$0")" && pwd -P)"
echo "SCRIPT_DIR: ${SCRIPT_DIR}"

PATH_PREFIX_VALUE=$("${SCRIPT_DIR}"/get_prefix.sh)
# This path must fit the one given to the build system where binaries are provided.
PREFIX=${PATH_PREFIX_VALUE}
echo "PREFIX: ${PREFIX}"
if [[ ! -e ${PREFIX} ]]; then
    echo "ERROR: Required path PREFIX for installed binaries not found."
    echo "Maybe the project has not yet been built or the build directory was moved or deleted."
    echo "The path to cr_logger and cr_verifier is PREFIX/bin"
    echo "You can try to set PREFIX manually here in the script instead of using get_prefix.sh"
    echo "Example: PREFIX=${HOME}/Software/install"
    echo "FAIL"
    exit 1
fi

# ----------------------------------------------------------------------
# -- Function to traverse upwards and locate the root project directory
find_project_root() {
    local dir="$1"
    # Keep climbing up until we hit the system root
    while [[ ${dir} != "/" ]] && [[ -n ${dir} ]]; do
        if [[ -d "${dir}/modules/secure-logging/tests" ]]; then
            echo "${dir}"
            return 0
        fi
        dir="$(dirname "${dir}")"
    done
    return 1
}

# -- Get paths to run this script position independently -----

# Try to find the root starting from the Current Working Directory ($PWD)
# Fallback to the Script's location ($SCRIPT_DIR) if PWD is outside the tree.
ROOT_DIR=$(find_project_root "${PWD}") || ROOT_DIR=$(find_project_root "${SCRIPT_DIR}") || true
echo "ROOT_DIR: ${ROOT_DIR}"
if [[ -z ${ROOT_DIR} ]]; then
    echo "Error: Could not locate the project root containing 'modules/secure-logging/tests'." >&2
    exit 1
fi
# Define our target variables relative to the dynamically found root
BUILD_DIR="${ROOT_DIR}/build"
MODULES_DIR="${ROOT_DIR}/modules"
echo "BUILD_DIR: ${BUILD_DIR}"
echo "MODULES_DIR: ${MODULES_DIR}"
# Ensure the modules folder actually exists
if [[ ! -d ${MODULES_DIR} ]]; then
    echo "Error: 'modules' directory does not exist at expected path: ${MODULES_DIR}" >&2
    exit 1
fi
IS_CRASH_RECOVERY="false"
if [[ -d ${MODULES_DIR}/secure-logging/crashrecovery ]]; then
    IS_CRASH_RECOVERY="true"
    echo "Info: Source folder contains crashrecovery"
fi

# -- ARTIFACTS: full file name of binaries to analyse ---
ARTIFACTS=(
    "${PREFIX}/sbin/syslog-ng"
    "${PREFIX}/sbin/syslog-ng-ctl"
    "${PREFIX}/bin/loggen"
    "${PREFIX}/bin/slogkey"
    "${PREFIX}/bin/slogencrypt"
    "${PREFIX}/bin/slogverify"
    "${PREFIX}/bin/loggen"
    "${PREFIX}/sbin/syslog-ng-ctl"
    "${PREFIX}/sbin/syslog-ng"
)

if [[ ${IS_CRASH_RECOVERY} == "true" ]]; then
    # Note: In case the install folder contains build artifacts
    # from a build where crash recovery was not avilable,
    # this script will return an error.
    ARTIFACTS+=("${PREFIX}/bin/cr_logger" "${PREFIX}/bin/cr_verifier")
fi

BIN=${PREFIX}/bin
SBIN=${PREFIX}/sbin

# -- current Git branch for build log name ---
GIT_BRANCH=""
echo " "
if git rev-parse --is-inside-work-tree >/dev/null 2>&1; then
    GIT_BRANCH=$(git rev-parse --abbrev-ref HEAD)
else
    echo "Not a Git repository."
fi
if [[ -z ${GIT_BRANCH} ]]; then
    GIT_BRANCH="unkown-git-branch"
else
    echo "GIT_BRANCH: ${GIT_BRANCH}"
fi

# -- Get full file name of log ---
# This should be available when aliasse form README.md are in ~/.bashrc
LOGFILE=""
echo "Check if TESTBAK_DIR has been exported in ~/.bashrc where aliasse form syslog-ng dev are provided"
echo "TESTBAK_DIR: ${TESTBAK_DIR}"
if [[ -z ${TESTBAK_DIR} ]]; then
    echo "TESTBAK_DIR is not defined, use default log path"
    SUBFOLDER_TEST="test_slog"
    HOME_BACKUP=${HOME}/${SUBFOLDER_TEST}/${SCRIPTNAME}
else
    echo "TESTBAK_DIR is defined"
    HOME_BACKUP=${TESTBAK_DIR}/${SCRIPTNAME}
fi
echo "HOME_BACKUP: ${HOME_BACKUP}"
mkdir -p "${HOME_BACKUP}"
BUILD_INFO_FILE=${PREFIX}/build_info.txt
# Read the first line of the file
first_line=$(head -n 1 "${BUILD_INFO_FILE}")
# Check if the first line is equal to "CMake"
SEARCH_GCC="-DCMAKE_C_COMPILER=gcc"
SEARCH_CLANG="-DCMAKE_C_COMPILER=clang"
if [[ ${first_line} == "CMake" ]]; then
    LOGFILE="${HOME_BACKUP}/${NOW}_${GIT_BRANCH}_CMake_binaries.txt"
    if grep -qF -- "${SEARCH_GCC}" "${BUILD_INFO_FILE}"; then
        LOGFILE="${HOME_BACKUP}/${NOW}_${GIT_BRANCH}_CMake_gcc_binaries.txt"
    else
        if grep -qF -- "${SEARCH_CLANG}" "${BUILD_INFO_FILE}"; then
            LOGFILE="${HOME_BACKUP}/${NOW}_${GIT_BRANCH}_CMake_clang_binaries.txt"
        fi
    fi
else
    LOGFILE="${HOME_BACKUP}/${NOW}_${GIT_BRANCH}_Autotools_binaries.txt"
fi

echo "LOGFILE: ${LOGFILE}"
echo "File: ${LOGFILE}" >"${LOGFILE}"
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
    echo "HOME_BACKUP: ${HOME_BACKUP}"

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

# ----------------------------------------------------------------------
# -- function to check artifacts in given argument
# Usage: check_artifacts "path1" "path2" "path3" ...
check_artifacts() {
    local cnt_missing=0
    local path
    for path in "$@"; do
        if [[ ! -e ${path} ]]; then
            echo "ERROR: Required file '${path}' not found." 2>&1 | tee -a "${LOGFILE}"
            ((cnt_missing++))
        else
            {
                echo "sha256sum ${path}"
                sha256sum "${path}"
                echo " "
                echo "${path} --help"
                "${path}" --help
                echo " "
                echo "ldd ${path}"
                ldd "${path}"
                echo " "
                echo "nm ${path}"
                nm "${path}"
                echo " "
                echo "objdump -p ${path}"
                objdump -p "${path}"
            } 2>&1 | tee -a "${LOGFILE}"
        fi
    done
    # Return 0 if all exist, or 1 if any are missing
    [[ ${cnt_missing} -eq 0 ]]
}

echo " "
"${SCRIPT_DIR}"/get_git_info.sh

# -- do initial checks -----
check_missing "${PREFIX}" "${BIN}" "${SBIN}"
check_missing "${HOME_BACKUP}"
check_missing "${LOGFILE}"

# -- list binaries ---
{
    echo " "
    echo "ls -alt ${PREFIX}/lib/"
    ls -alt "${PREFIX}/lib/"
} 2>&1 | tee -a "${LOGFILE}"

{
    echo " "
    echo "ls -alt ${PREFIX}/sbin/"
    ls -alt "${PREFIX}/sbin/"
} 2>&1 | tee -a "${LOGFILE}"

{
    echo " "
    echo "ls -alt ${PREFIX}/bin/"
    ls -alt "${PREFIX}/bin/"
} 2>&1 | tee -a "${LOGFILE}"
echo " "

# -- call ldd for each binary ---
if check_artifacts "${ARTIFACTS[@]}"; then
    echo "Success: All aritfacts found." 2>&1 | tee -a "${LOGFILE}"
else
    echo "Failure: Not all artifacts founds." 2>&1 | tee -a "${LOGFILE}"
    cnt_error=$((cnt_error + 1))
fi

# build info
{
    echo " "
    cat "${BUILD_INFO_FILE}"
    echo " "
} 2>&1 | tee -a "${LOGFILE}"

echo "return cnt_error: ${cnt_error}"
#if ((cnt_error == 0)); then
if [[ ${cnt_error} -eq 0 ]]; then
    echo "PASS"
else
    echo "Found ERROR"
    echo "FAIL"
fi

echo "FILE:"
echo "${LOGFILE}"
echo " "
echo Done
echo " "
# exit "${cnt_error:-0}"
exit "${cnt_error}"
