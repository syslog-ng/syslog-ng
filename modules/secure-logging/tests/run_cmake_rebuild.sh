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
# File:   run_cmake_rebuild.sh
# Author: Airbus Commercial Aircraft <secure-logging@airbus.com>
# Date:   2026-06-30
#
# Helper script to rebuild all from scratch inclusive installation and
# test.
#
# Usage: ./run_cmake_rebuild.sh [b|i|t]
#        (no argument) : Execute all tasks (complete rebuild, install, test)
#        b             : Execute only complete rebuild and install
#        i             : Execute only install
#        t             : Execute only install and test
# ----------------------------------------------------------------------

set -o pipefail

#-- build environment (if set somewhere else, take over value) ---

#-- clang (if false then gcc is used)
: "${IS_CLANG:="false"}"

#-- make (can become qmake if defined outside)
: "${MY_MAKE:="make"}"

#-- parallel build
: "${IS_PARALLEL_BUILD:="true"}"

#-- installation directory (most likely exported in ~/.bashrc)
: "${SW_INSTALL_DIR:="$HOME/Software/install"}"

#-- build logs directory (most likely exported in ~/.bashrc)
: "${BUILD_LOG_DIR:="$HOME/backup/05_build_log"}"

# When user wants to keep installation directory, REMOVE_PREFIX must be set to false
REMOVE_PREFIX="true"

PREFIX=${SW_INSTALL_DIR}
LOGS=${BUILD_LOG_DIR}

# The following is needed on some systems in context of tests with valgrind.
# When valgrind is not used, this flag can be set to false.
# Ensure old DWARF 4 format (used only when clang for DBG_FORMAT)
IS_DWARF_4="true"

IS_SPECIAL_FLAGS="false" # If Ubuntu Linux is used, special flags for clang are needed

export AM_COLOR_TESTS=always
export FORCE_COLOR=1

NOW=$(date +%Y-%m-%d_%H%M_%S)
START_TIME=$(date +%s)
INFO="Running command: "
FAIL_CD="Error: Failed to change directory"
FAIL_CDR="Error: Failed to restore original directory: "

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

LOGFILE="${LOGS}/${NOW}_buildlog_${GIT_BRANCH}_"

# -- Check for Criterion (Unit Test Framework) ---
CRITERION_CFLAGS=""
CRITERION_LDFLAGS=""

if pkg-config --exists criterion; then
    CRITERION_CFLAGS=$(pkg-config --cflags criterion)
    CRITERION_LDFLAGS=$(pkg-config --libs criterion)
    echo "Criterion found via pkg-config."
else
    echo "Criterion not found via pkg-config, relying on system defaults."
fi

if [[ ${IS_CLANG} == "true" ]]; then
    C_COMPILER="clang"
    CXX_COMPILER="clang++"
    # Clang-specific flags
    EXTRA_FLAGS="-stdlib=libc++"
else
    C_COMPILER="gcc"
    CXX_COMPILER="g++"
    # GCC doesn't need the stdlib flag
    EXTRA_FLAGS=""
fi

if [[ ${IS_SPECIAL_FLAGS} == "false" ]]; then
    EXTRA_FLAGS=""
fi

# Define your warning flags. Those do work for both GCC and Clang.
# WARNING_FLAGS="-Wall -Wextra -Wshadow -Wpedantic"
# WARNING_FLAGS="-Wall -Wextra -Wshadow"
WARNING_FLAGS="-Wall -Wextra -Wshadow -Wconversion"

BUILD_MODE="debug" # Options: "debug" or "release"

if [[ ${BUILD_MODE} == "debug" ]]; then
    MY_DEBUG_FLAGS="-g"
    MY_OPT_FLAGS="-O0" # No optimization, easy debugging
else
    MY_DEBUG_FLAGS=""  # Correct! Empty string for release
    MY_OPT_FLAGS="-O3" # High optimization for production
fi

# -- Valgrind workaround -----
# On jetson valgrind did not work with binaries from clang build process.
# The reason was that clang was using a newer Debug Format which is not
# known by valgrind.
# clang --version: Ubuntu clang version 14.0.0-1ubuntu1.1, Target: aarch64-unknown-linux-gnu
# gcc --version: gcc (Ubuntu 11.4.0-1ubuntu1~22.04.3) 11.4.0
# glib-compile-schemas --version: 2.72.4
# valgrind --version: valgrind-3.18.1
# Valgrind might not work together with binaries created by clang.
# ### unhandled dwarf2 abbrev form code 0x25
# ### unhandled dwarf2 abbrev form code 0x1b
# ==251082== Valgrind: debuginfo reader: ensure_valid failed:
# ...
# ==251082== Valgrind: debuginfo reader: Possibly corrupted debuginfo file.
# ==251082== Valgrind: I can't recover.  Giving up.  Sorry.
#
# Therefore ensure old DWARF 4 format like gcc does for debug
# Force DWARF 4 version for debug symbols
# -DCMAKE_CXX_FLAGS="-gdwarf-4" -DCMAKE_C_FLAGS="-gdwarf-4"

DBG_FORMAT=""
if [[ ${IS_DWARF_4} == "true" && ${IS_CLANG} == "true" ]]; then
    DBG_FORMAT="-gdwarf-4"
fi

CONFIGURATION=(
    -DCMAKE_C_COMPILER="${C_COMPILER}"
    -DCMAKE_CXX_COMPILER="${CXX_COMPILER}"
    -DCMAKE_C_FLAGS="${MY_DEBUG_FLAGS} ${MY_OPT_FLAGS} ${WARNING_FLAGS} ${EXTRA_FLAGS} ${CRITERION_CFLAGS}"
    -DCMAKE_CXX_FLAGS="${MY_DEBUG_FLAGS} ${MY_OPT_FLAGS} ${WARNING_FLAGS} ${EXTRA_FLAGS} ${CRITERION_CFLAGS}"
    -DCMAKE_C_FLAGS_DEBUG="-g ${DBG_FORMAT}"
    -DCMAKE_CXX_FLAGS_DEBUG="-g ${DBG_FORMAT}"
    -DCMAKE_BUILD_TYPE=None # Tells CMake not to inject its own default flags
    -DCMAKE_EXE_LINKER_FLAGS="${EXTRA_FLAGS} ${CRITERION_LDFLAGS}"
    -DIVYKIS_SOURCE=system
    -DJSONC_SOURCE=system
    -DENABLE_SLOG=on
    -DENABLE_MANPAGES=on
    -DENABLE_PYTHON=off
    -DENABLE_APPMODEL=on
    -DENABLE_PYTHON_MODULES=off
    -DENABLE_JAVA=off
    -DENABLE_PYTHON_JAVA=off
    -DENABLE_CLOUD_AUTH=off
    -DENABLE_CLOUD_AUTH_CURL=off
    -DENABLE_AZURE_AUTH_HEADER=off
    -DENABLE_STOMP=off
    -DENABLE_EXAMPLE_FILTERX_FUNC=off
    -DENABLE_EXAMPLE_MODULES=off
    -DENABLE_XML=off
    -DENABLE_STARDATE=off
    -DCMAKE_BUILD_TYPE=Debug
    -DCMAKE_INSTALL_PREFIX="${PREFIX}"
)

# -- The confirmation prompt ---
echo " "
echo "PREFIX (where to install built binaries): ${PREFIX}"
echo " "
echo "CONFIGURATION:"
for option in "${CONFIGURATION[@]}"; do
    echo "  ${option}"
done
echo " "
echo "LOGS: ${LOGS}"
echo " "
echo "LOGFILE: ${LOGFILE}"
echo " "

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

# build folder might not exists yet
# Explicitly verify that the 'build' folder is on the same level as 'modules'
if [[ -d ${BUILD_DIR} ]]; then
    BUILD_PARENT="$(dirname "$(readlink -f "${BUILD_DIR}")")"
    MODULES_PARENT="$(dirname "$(readlink -f "${MODULES_DIR}")")"
    if [[ ${BUILD_PARENT} != "${MODULES_PARENT}" ]]; then
        echo "Safety Error: Validation failed. The build folder is not on the same level as the modules folder!" >&2
        exit 1
    fi
fi

IS_CRASH_RECOVERY="false"
if [[ -d ${MODULES_DIR}/secure-logging/crashrecovery ]]; then
    IS_CRASH_RECOVERY="true"
fi

ARTIFACTS=(
    "${PREFIX}/sbin/syslog-ng"
    "${PREFIX}/sbin/syslog-ng-ctl"
    "${PREFIX}/bin/loggen"
    "${PREFIX}/bin/slogkey"
    "${PREFIX}/bin/slogencrypt"
    "${PREFIX}/bin/slogverify"
)

if [[ ${IS_CRASH_RECOVERY} == "true" ]]; then
    ARTIFACTS+=("${PREFIX}/bin/cr_logger" "${PREFIX}/bin/cr_verifier")
fi

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

# -- start build with cmake ---
echo "Build environment: CMake and ${C_COMPILER}"

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
perform_complete_rebuild() {
    set -e
    ORIGINAL_DIR=$(pwd)

    # -- Clean up ---
    echo " "
    echo "Artifacts from previous build, inclusive installed binaries, will be removed now .."
    echo "perform_complete_rebuild, BUILD_DIR: ${BUILD_DIR}, PREFIX: ${PREFIX}"

    set +e
    if [[ -d ${BUILD_DIR} ]]; then
        cd "${BUILD_DIR}" || {
            echo "${FAIL_CD}"
            cd "${ORIGINAL_DIR}" || echo "${FAIL_CDR} ${ORIGINAL_DIR}" >&2
            exit 1
        }
        echo "${INFO} ${MY_MAKE} clean"
        ${MY_MAKE} clean
        echo "${INFO} ${MY_MAKE} distclean"
        ${MY_MAKE} distclean
    fi
    cd "${ROOT_DIR}" || {
        echo "Failed to change directory"
        cd "${ORIGINAL_DIR}" || echo "${FAIL_CDR} ${ORIGINAL_DIR}" >&2
        exit 1
    }
    clean_targets "${ARTIFACTS[@]}"
    clean_targets "${ROOT_DIR}/aclocal.m4" "${ROOT_DIR}/autom4te.cache" "${ROOT_DIR}/config.h.in" \
        "${ROOT_DIR}/configure" "${ROOT_DIR}/Makefile.in"
    if [[ -d ${BUILD_DIR} ]]; then
        echo "${INFO} rm -rf ${BUILD_DIR}"
        rm -rf "${BUILD_DIR}"
    fi
    set -e

    echo " "
    echo "${INFO} mkdir -pv ${BUILD_DIR}"
    mkdir -pv "${BUILD_DIR}"

    # remove install directory
    if [[ ${REMOVE_PREFIX} == "true" ]]; then
        if [[ -n ${PREFIX} && -d ${PREFIX} ]]; then
            echo "${INFO} rm -rf ${PREFIX}"
            rm -rf "${PREFIX}"
        fi
    fi

    echo " "
    echo "${INFO} mkdir -pv ${PREFIX}"
    mkdir -pv "${PREFIX}"

    echo " "
    echo "${INFO} mkdir -pv ${LOGS}"
    mkdir -pv "${LOGS}"

    echo "Check precondition"
    all_files_exist "${BUILD_DIR}" "${PREFIX}" "${LOGS}"

    # -- cmake ---
    cd "${BUILD_DIR}" || {
        echo "${FAIL_CD}"
        cd "${ORIGINAL_DIR}" || echo "${FAIL_CDR} ${ORIGINAL_DIR}" >&2
        exit 1
    }
    CURRENT_LOG_CM="${LOGFILE}_CMake_${C_COMPILER}.txt"
    echo "File: ${CURRENT_LOG_CM}" >"${CURRENT_LOG_CM}"
    # Temporarily disable exit-on-error to capture cmake's exit code
    set +e
    echo "${INFO} cmake .."
    cmake --log-level=DEBUG "${CONFIGURATION[@]}" .. 2>&1 | tee -a "${CURRENT_LOG_CM}"
    CMAKE_EXIT_CODE=${PIPESTATUS[0]}
    set -e # Re-enable exit-on-error

    if [[ ${CMAKE_EXIT_CODE} -ne 0 ]]; then
        echo " "
        echo "cmake failed! Aborting build script."
        echo " "
        cd "${ORIGINAL_DIR}" || echo "${FAIL_CDR} ${ORIGINAL_DIR}" >&2
        exit 1
    fi

    # -- make ---
    set -e
    JOBS=""
    if [[ ${IS_PARALLEL_BUILD} == "true" ]]; then
        JOBS="-j$(nproc)"
    fi
    cd "${BUILD_DIR}" || {
        echo "${FAIL_CD}"
        cd "${ORIGINAL_DIR}" || echo "${FAIL_CDR} ${ORIGINAL_DIR}" >&2
        exit 1
    }
    echo "pwd: $(pwd || true)"
    echo "${INFO} ${MY_MAKE}"
    CURRENT_LOG_MAKE="${LOGFILE}_CMake_${C_COMPILER}_make.txt"
    echo "File: ${CURRENT_LOG_MAKE}" >"${CURRENT_LOG_MAKE}"

    set +e
    if [[ ${IS_PARALLEL_BUILD} == "true" ]]; then
        ${MY_MAKE} "${JOBS}" VERBOSE=1 2>&1 | tee -a "${CURRENT_LOG_MAKE}"
    else
        ${MY_MAKE} VERBOSE=1 2>&1 | tee -a "${CURRENT_LOG_MAKE}"
    fi
    # Check PIPESTATUS of the make command (index 0)
    MAKE_RET=${PIPESTATUS[0]}
    set -e # Re-enable exit-on-error

    if [[ ${MAKE_RET} -ne 0 ]]; then
        echo "--------------------------------------------------------"
        echo "Searching for 'error:' in log ..."

        # -i: case insensitive
        # -m 1: Stop after the first match (prevents the 3x repeat)
        # -A 2: Show 2 lines of context
        # -B 1: Show 1 line before (to see the filename and line number)
        # "error: #error" "${CURRENT_LOG}"
        grep -m 1 -i -A 2 -B 1 "error:" "${CURRENT_LOG_MAKE}"

        # 2. Search for linker/spelling errors (undefined references)
        # We check if grep finds anything; if it does, we print a header
        if grep -q "undefined reference to" "${CURRENT_LOG_MAKE}"; then
            echo "--- Linker Error Detected (Possible Spelling/Missing Lib) ---"
            grep -m 1 -i -A 1 -B 1 "undefined reference to" "${CURRENT_LOG_MAKE}"
        fi

        echo " "
        echo "Build log file:"
        echo "${CURRENT_LOG_MAKE}"
        echo "--------------------------------------------------------"
        echo "${MY_MAKE} failed! Aborting build script."
        cd "${ORIGINAL_DIR}" || echo "${FAIL_CDR} ${ORIGINAL_DIR}" >&2
        exit "${MAKE_RET}"
    fi
    cd "${ORIGINAL_DIR}" || echo "${FAIL_CDR} ${ORIGINAL_DIR}" >&2
}

# ----------------------------------------------------------------------
perform_install() {
    set -e
    ORIGINAL_DIR=$(pwd)
    cd "${BUILD_DIR}" || {
        echo "${FAIL_CD}"
        exit 1
    }
    echo "pwd: $(pwd || true)"

    echo "${INFO} ${MY_MAKE} install"
    CURRENT_LOG_INSTALL="${LOGFILE}_CMake_${C_COMPILER}_make_install.txt"
    echo "File: ${CURRENT_LOG_INSTALL}" >"${CURRENT_LOG_INSTALL}"

    set +e
    ${MY_MAKE} install 2>&1 | tee -a "${CURRENT_LOG_INSTALL}"
    INSTALL_EXIT_CODE=${PIPESTATUS[0]}
    set -e

    # -- provide info of build system ---
    INFO_FILE="${PREFIX}/build_info.txt"
    echo "CMake" >"${INFO_FILE}"
    {
        echo " "
        echo "Build time: ${NOW}"
        echo " "
        cat /etc/os-release
        echo " "
        echo "Build configuration:"
        for arg in "${CONFIGURATION[@]}"; do
            echo "${arg}"
        done
        echo "BUILD_MODE: ${BUILD_MODE}"
        echo "MY_DEBUG_FLAGS: ${MY_DEBUG_FLAGS}"
        echo "MY_OPT_FLAGS: ${MY_OPT_FLAGS}"
        echo " "
        if [[ ${IS_CLANG} == "true" ]]; then
            echo "clang --version"
            clang --version
        else
            echo "gcc --version"
            gcc --version
        fi
        echo " "
        echo "${MY_MAKE} --version "
        ${MY_MAKE} --version
        echo " "
        echo "autoconf --version"
        autoconf --version
        echo " "
        echo "automake --version"
        automake --version
        echo " "
        echo "glib-compile-schemas --version"
        glib-compile-schemas --version
        echo " "
        echo "openssl version"
        openssl version
    } >>"${INFO_FILE}"

    if [[ ${INSTALL_EXIT_CODE} -ne 0 ]]; then
        echo " "
        echo "${MY_MAKE} install failed! Aborting build script."
        echo " "
        cd "${ORIGINAL_DIR}" || echo "${FAIL_CDR} ${ORIGINAL_DIR}" >&2
        exit 1
    fi

    # -- check install ---
    echo " "
    echo "ls -alt ${PREFIX}/lib/"
    ls -alt "${PREFIX}/lib/"

    echo " "
    echo "ls -alt ${PREFIX}/sbin/"
    ls -alt "${PREFIX}/sbin/"

    echo " "
    echo "ls -alt ${PREFIX}/bin/"
    ls -alt "${PREFIX}/bin/"

    echo " "
    echo "Now checking whether binaries have been installed in ${PREFIX}/..."

    set +e
    # check module/secure-logging artifacts
    if all_files_exist "${ARTIFACTS[@]}"; then
        echo "Success: Build was successful. All required binaries were found."
    else
        echo "Failure: Build or/and install failed. At least one required binary was missing."
        echo "Not all artifacts found. Installation failed! Aborting build script."
        echo " "
        cd "${ORIGINAL_DIR}" || echo "${FAIL_CDR} ${ORIGINAL_DIR}" >&2
        exit 1
    fi
    set -e
    cd "${ORIGINAL_DIR}" || echo "${FAIL_CDR} ${ORIGINAL_DIR}" >&2
}

# ----------------------------------------------------------------------

perform_test() {
    ORIGINAL_DIR=$(pwd)
    set -e
    cd "${BUILD_DIR}" || {
        echo "${FAIL_CD}"
        exit 1
    }
    echo "pwd: $(pwd || true)"

    echo "${INFO} ${MY_MAKE} check"
    CURRENT_LOG_CHECK="${LOGFILE}_cmake_${C_COMPILER}_make_check.txt"
    echo "File: ${CURRENT_LOG_CHECK}" >"${CURRENT_LOG_CHECK}"

    set +e
    ${MY_MAKE} check 2>&1 | tee -a "${CURRENT_LOG_CHECK}"
    CHECK_EXIT_CODE=${PIPESTATUS[0]}
    set -e
    if [[ ${CHECK_EXIT_CODE} -ne 0 ]]; then
        echo " "
        echo "check failed!"
        echo " "
        cd "${ORIGINAL_DIR}" || echo "${FAIL_CDR} ${ORIGINAL_DIR}" >&2
        exit 1
    fi
    cd "${ORIGINAL_DIR}" || echo "${FAIL_CDR} ${ORIGINAL_DIR}" >&2
}

case "$1" in
# The argument is an empty string ""
# This happens when the script is run with no arguments.
"")
    echo "No argument provided. Running ALL tasks."
    echo "-------------------------------------"
    perform_complete_rebuild
    perform_install
    perform_test
    ;;

    # The argument is exactly "b"
"b")
    echo "Argument 'b' detected. Only rebuild and install (without test)."
    echo "-------------------------------------"
    perform_complete_rebuild
    perform_install
    ;;

    # The argument is exactly "i"
"i")
    echo "Argument 'i' detected. Only install."
    echo "-------------------------------------"
    perform_install
    ;;

    # The argument is exactly "t"
"t")
    echo "Argument 't' detected. Running only install and test."
    echo "-------------------------------------"
    perform_install
    perform_test
    ;;

# Any other argument (*)
# This is a catch-all for invalid inputs.
*)
    echo "ERROR: Invalid argument '$1'."
    echo "Usage: $0 [b|t]"
    echo "  (no argument) : Execute all tasks (complete rebuild, install, test)."
    echo "  b             : Execute only complete rebuild and install."
    echo "  i             : Execute only install."
    echo "  t             : Execute only install and test."
    exit 1 # Exit with an error status
    ;;
esac

END=$(date +%Y-%m-%d_%H%M_%S)
END_TIME=$(date +%s)
echo "Start time: ${NOW}"
echo "Finished  : ${END}"
DURATION=$((END_TIME - START_TIME))
duration_min=$((DURATION / 60))
duration_sec=$((DURATION % 60))
host_name=$(uname -n)
echo "Build environment: CMake and ${C_COMPILER}"
echo "Total execution time on ${host_name}: ${duration_min} minutes and ${duration_sec} seconds"
