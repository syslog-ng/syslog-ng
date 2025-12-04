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

#-----------------------------------------------------------------------
# File:   rebuildall.sh
# Author: Airbus Commercial Aircraft <secure-logging@airbus.com>
# Date:   2025-12-04
#
# Helper script to rebuild all from scratch inclusive installation and
# test.
#
# Usage: ./rebuildall.sh [b|t]
#        (no argument) : Execute all tasks (complete rebuild, install, test)
#        b             : Execute only complete rebuild and install
#        i             : Execute only install
#        t             : Execute only install and test
# ----------------------------------------------------------------------

PREFIX=${HOME}/Software/install

# LOGS=${HOME}/temp
# LOGS=/tmp
LOGS=${HOME}/backup

NOW=$(date +%Y-%m-%d_%H%M_%S)
START_TIME=$(date +%s)
INFO="Running command: "

# configure is called from inside ./build/ directory
CONFIGURE_CMD="../configure --prefix=${PREFIX} --enable-debug \
 --enable-manpages --disable-java \
 --disable-python --disable-python-modules --with-ivykis=system --with-jsonc=yes \
 --enable-stomp=no --enable-sql=no"

# current Git branch for build log name
GIT_BRANCH=""
echo " "
if git rev-parse --is-inside-work-tree >/dev/null 2>&1; then
    GIT_BRANCH=$(git rev-parse --abbrev-ref HEAD)
else
    echo "Not a Git repository."
fi
if [ -z "${GIT_BRANCH}" ]; then
    GIT_BRANCH="unkown-git-branch"
else
    echo "GIT_BRANCH: ${GIT_BRANCH}"
fi
LOGFILE="${LOGS}/buildlog_${GIT_BRANCH}_${NOW}_"

# -- The confirmation prompt ---
echo " "
echo "PREFIX (for configure, where to install binaries): ${PREFIX}"
echo " "
echo "CONFIGURE_CMD: ${CONFIGURE_CMD}"
echo " "
echo "LOGS: ${LOGS}"
echo " "
echo "LOGFILE: ${LOGFILE}"
echo " "
echo "Old binaries and build directories will be deleted without warning!"
echo " "
echo "Settings will be applied. Are you sure?"
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
    if [ "$#" -eq 0 ]; then
        echo "Usage: clean_targets <path1> [path2] ..."
        return 1
    fi

    # Loop through all the arguments passed to the function
    for target in "$@"; do
        # Check if the file or directory exists (-e handles both)
        if [ -e "${target}" ]; then
            echo "Deleting ${target}..."
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
    for path in "$@"; do
        # Check if the path does NOT exist (-e works for files and directories)
        if [ ! -e "${path}" ]; then
            echo "Error: Required path '${path}' not found."
            return 0 # Return 0 immediately if any file is missing
        fi
    done
    # If the loop completes, all paths were found
    return 1 # SUCCESS all files or folders found
}

# ----------------------------------------------------------------------
perform_complete_rebuild() {

    # -- check whether ./autogen.sh exists and is executable ---
    AUTOGEN_SCRIPT="./autogen.sh"
    if ! [ -f "${AUTOGEN_SCRIPT}" ]; then
        echo "ERROR: Script '${AUTOGEN_SCRIPT}' not found!" >&2
        exit 1
    fi
    if ! [ -x "${AUTOGEN_SCRIPT}" ]; then
        echo "ERROR: '${AUTOGEN_SCRIPT}' is not executable!"
        echo "Use chmod +x ${AUTOGEN_SCRIPT} to fix this and recall this script again."
        exit 1
    fi

    mkdir -p "${PREFIX}"
    mkdir -p "${LOGS}"
    all_files_exist "${PREFIX}" "${LOGS}"

    # -- Clean up ---
    echo " "
    echo "Artifacts from previous build, inclusive installed binaries, will be removed now .."

    clean_targets "./build" "./aclocal.m4" "./autom4te.cache" "./config.h.in" \
        "./configure" "./Makefile.in" "${PREFIX}/sbin/syslog-ng" "${PREFIX}/sbin/syslog-ng-ctl" \
        "${PREFIX}/bin/loggen" "${PREFIX}/bin/pdbtool" "${PREFIX}/bin/perist-tool" \
        "${PREFIX}/bin/slogencrypt" "${PREFIX}/bin/slogverify"

    echo " "
    echo "${INFO} rm -rf ./build"
    rm -rf ./build

    echo " "
    echo "${INFO} mkdir build"
    mkdir build

    echo " "
    echo "${INFO} ./autogen.sh"
    ./autogen.sh 2>&1 | tee "${LOGFILE}"_autogen.txt

    (
        cd build &&
            echo "${INFO} ../configure" &&
            # CFLAGS="-Wall -fanalyzer" ${CONFIGURE_CMD} 2>&1 | tee "${LOGFILE}"_configure.txt
            CFLAGS="-Wall" ${CONFIGURE_CMD} 2>&1 | tee "${LOGFILE}"_configure.txt
    )

    (
        cd build &&
            echo "${INFO} make" &&
            make V=1 2>&1 | tee "${LOGFILE}_make.txt"
    )
}

# ----------------------------------------------------------------------
perform_install() {
    (
        cd build &&
            echo "${INFO} make install" &&
            make install 2>&1 | tee "${LOGFILE}_make_install.txt"
    )

    # -- check install ---
    sleep 1
    echo " "
    echo "Now checking whether binaries have been installed in ${PREFIX}/..."
    all_files_exist "${PREFIX}/sbin/syslog-ng" "${PREFIX}/sbin/syslog-ng-ctl" \
        "${PREFIX}/bin/loggen" "${PREFIX}/bin/slogencrypt" \
        "${PREFIX}/bin/slogverify" "${PREFIX}/bin/slogverify"

    if [ $? -eq 1 ]; then
        echo " "
        echo "Success: Build was successful. All required binaries were found."
    else
        echo " "
        echo "Failure: Build or/and install failed. At least one required binary was missing."
    fi
}

# ----------------------------------------------------------------------
perform_test() {
    (
        cd build &&
            echo "${INFO} make check" &&
            make check 2>&1 | tee "${LOGFILE}_make_check.txt"
    )
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
echo "Total execution time on ${host_name}: ${duration_min} minutes and ${duration_sec} seconds."
