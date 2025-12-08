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
# File:   get_prefix.sh
# Date:   2025-12-08
#
# This script is expected being called from inside root of
# syslog-ng source directory after the build process has finished.
# The returned path should be the install path of binaries.
#
# See --prefix argument of configure script
#       e.g.: ../configure --prefix=/home/johndoe/Software/install

# set -x

SCRIPT_DIR="$(cd -- "$(dirname -- "$0")" && pwd -P)"
SEARCH_DIR="${SCRIPT_DIR}/../../../"
CONFIG_FILE=$(find "${SEARCH_DIR}" -name "config.h" -type f -exec \
    grep -l "^#define PATH_PREFIX" {} + | head -n 1)

if [ -n "${CONFIG_FILE}" ]; then
    PATH_PREFIX_VALUE=$(grep '#define PATH_PREFIX' "${CONFIG_FILE}" | sed 's/.*"\([^"]*\)".*/\1/')
    echo "${PATH_PREFIX_VALUE}"
else

    CMAKE_INSTALL_FILE=$(find "${SEARCH_DIR}" -name "cmake_install.cmake" -type f | head -n 1)

    if [ -n "${CMAKE_INSTALL_FILE}" ]; then
        INSTALL_PATH=$(grep 'set(CMAKE_INSTALL_PREFIX' "${CMAKE_INSTALL_FILE}" |
            sed -E 's/set\(CMAKE_INSTALL_PREFIX "(.*)"\)/\1/')

        # Check if the path was successfully extracted
        if [ -n "${INSTALL_PATH}" ]; then
            echo "${INSTALL_PATH}"
        else
            echo "Error: Found 'cmake_install.cmake', but could not extract CMAKE_INSTALL_PREFIX." >&2
            exit 1
        fi
    else
        # Neither config.h nor cmake_install.cmake was found
        echo "Error: No suitable 'config.h' or 'cmake_install.cmake' file was found to get installation path from." >&2
        exit 1
    fi

fi
