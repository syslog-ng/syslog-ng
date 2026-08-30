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

#-----------------------------------------------------------------------
# File:   run_clean.sh
# Author: Airbus Commercial Aircraft <secure-logging@airbus.com>
# Date:   2026-06-30
#
# Helper script to remove build artifact

# Usage: ./run_clean.sh
# ----------------------------------------------------------------------

set -o pipefail

: "${IS_CLANG:="false"}"
: "${MY_MAKE:="make"}"
: "${SW_INSTALL_DIR:="$HOME/Software/install"}"
: "${BUILD_LOG_DIR:="$HOME/backup/05_build_log"}"

# When user wants to keep installation directory, REMOVE_PREFIX must be false
REMOVE_PREFIX="true"

PREFIX=${SW_INSTALL_DIR}
LOGS=${BUILD_LOG_DIR}

export AM_COLOR_TESTS=always
export FORCE_COLOR=1

NOW=$(date +%Y-%m-%d_%H%M_%S)
START_TIME=$(date +%s)
INFO="Running command: "
FAIL_CD="Error: Failed to change directory"
# FAIL_CDR="Error: Failed to restore original directory: "

# current Git branch for build log name
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

LOGFILE="${LOGS}/${NOW}_buildlog_${GIT_BRANCH}_clean.txt"

echo " "
echo "Build directory will be deleted!"
if [[ ${REMOVE_PREFIX} == "true" ]]; then
    echo "The entire installation directory ${PREFIX} will be deleted."
fi
echo " "
echo "Settings will be applied. ARE YOU SURE?"
echo " "

printf "Please enter [y]es or [n]o: "
read -r response
case "${response}" in
[yY][eE][sS] | [yY])
    echo "Starting process..."
    ;;
*)
    echo "Operation cancelled by user."
    exit 1
    ;;
esac

# ----------------------------------------------------------------------
error_exit() {
    echo "ERROR: $1" >&2
    exit 1
}

# ----------------------------------------------------------------------
# -- helper function to delete files and directories
#    to remove old build artifacts
# Usage: clean_targets "path1" "path2" "path3" ...
clean_targets() {
    # Check if any arguments were provided
    if [[ $# -eq 0 ]]; then
        echo "Usage: clean_targets <path1> [path2] ..."
        return 1
    fi

    # Loop through all the arguments passed to the function
    for target in "$@"; do
        # Check if the file or directory exists (-e handles both)
        if [[ -e ${target} ]]; then
            echo "Deleting ${target}"
            # Use rm -rf to forcefully remove files and directories without prompting
            rm -rf "${target}"
        else
            echo "Skipping ${target} (does not exist)."
        fi
    done
}

# ----------------------------------------------------------------------
# -- helper function to check if all files in given array do exist
# Usage: all_files_exist "path1" "path2" "path3" ...
all_files_exist() {
    local cnt_missing=0
    local path
    for path in "$@"; do
        if [[ ! -e ${path} ]]; then
            echo "ERROR: Required file '${path}' not found." >&2
            ((cnt_missing++))
        fi
    done
    # Return 0 if all exist, or 1 if any are missing
    [[ ${cnt_missing} -eq 0 ]]
}

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

# ----------------------------------------------------------------------
perform_complete_clean() {
    echo "File: ${LOGFILE}" >"${LOGFILE}"
    (
        # -- Clean up ---
        echo " "
        echo "Artifacts from previous build, inclusive installed binaries, will be removed now .."

        # Grab the directory where the script itself is physically stored
        SCRIPT_DIR="$(dirname "$(readlink -f "${BASH_SOURCE[0]}")")"

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

        # -- SAFETY TESTS ---
        # Ensure the modules folder actually exists
        if [[ ! -d ${MODULES_DIR} ]]; then
            echo "Error: 'modules' directory does not exist at expected path: ${MODULES_DIR}" >&2
            exit 1
        fi
        # Explicitly verify that the 'build' folder is on the same level as 'modules'
        if [[ -d ${BUILD_DIR} ]]; then
            BUILD_PARENT="$(dirname "$(readlink -f "${BUILD_DIR}")")"
            MODULES_PARENT="$(dirname "$(readlink -f "${MODULES_DIR}")")"

            if [[ ${BUILD_PARENT} != "${MODULES_PARENT}" ]]; then
                echo "Safety Error: Validation failed. The build folder is not on the same level as the modules folder!" >&2
                exit 1
            fi

            # -- EXECUTION ---
            (
                echo "Before clean-up"
                du -sch "${ROOT_DIR}" || return
            )
            echo "Safely removing build directory at: ${BUILD_DIR}"
            (
                set +e
                if [[ -d ${BUILD_DIR} ]]; then
                    cd "${BUILD_DIR}" || {
                        echo "${FAIL_CD}"
                        return
                    }
                    echo "${INFO} ${MY_MAKE} clean"
                    ${MY_MAKE} clean
                    echo "${INFO} ${MY_MAKE} distclean"
                    ${MY_MAKE} distclean
                fi
                set -e
            )

            clean_targets "${ROOT_DIR}/aclocal.m4" "${ROOT_DIR}/autom4te.cache" "${ROOT_DIR}/config.h.in" \
                "${ROOT_DIR}/configure" "${ROOT_DIR}/Makefile.in"

            if [[ -n ${BUILD_DIR} && -d ${BUILD_DIR} ]]; then
                echo "${INFO} rm -rf ${BUILD_DIR}"
                rm -rf "${BUILD_DIR}"
            fi
            (
                cd "${ROOT_DIR}" || {
                    echo "${FAIL_CD}"
                    return
                }
                echo "ls -alt"
                ls -alt
            )

            (
                echo "After clean-up"
                du -sch "${ROOT_DIR}" || {
                    echo "failed du -sch ${ROOT_DIR}"
                    return
                }
            )
        else
            echo "Info: Build directory does not exist at ${BUILD_DIR}."
        fi

        # remove install directory
        if [[ ${REMOVE_PREFIX} == "true" ]]; then
            if [[ -n ${PREFIX} && -d ${PREFIX} ]]; then
                echo "${INFO} rm -rf ${PREFIX}"
                rm -rf "${PREFIX}"
            fi
        fi

    ) 2>&1 | tee -a "${LOGFILE}"

}

# -- clean ---
#
perform_complete_clean

END=$(date +%Y-%m-%d_%H%M_%S)
END_TIME=$(date +%s)
echo "Start time: ${NOW}"
echo "Finished  : ${END}"
DURATION=$((END_TIME - START_TIME))
duration_min=$((DURATION / 60))
duration_sec=$((DURATION % 60))
host_name=$(uname -n)
echo "Total execution time for clean on ${host_name}: ${duration_min} minutes and ${duration_sec} seconds."
