#!/bin/sh

#-----------------------------------------------------------------------
# Copyright (c) 2019-2025 Airbus Commercial Aircraft
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
#-----------------------------------------------------------------------

#-----------------------------------------------------------------------
# File:   get_prefix.sh
# Author: Airbus Commercial Aircraft <secure-logging@airbus.com>
# Date:   2025-11-25
#
# This script is expected being called from inside root of
# syslog-ng source directory after the build process has finished.
# The returned path should be the install path of binaries.
#
# See --prefix argument of configure script
#       e.g.: ../configure --prefix=/home/johndoe/Software/install
#-----------------------------------------------------------------------

SCRIPT_DIR="$(cd -- "$(dirname -- "$0")" && pwd -P)"
SEARCH_DIR="${SCRIPT_DIR}/../../../"
CONFIG_FILE=$(find "${SEARCH_DIR}" -name "config.h" -type f -exec \
    grep -l "^#define PATH_PREFIX" {} + | head -n 1)

if [ -n "${CONFIG_FILE}" ]; then
    PATH_PREFIX_VALUE=$(grep '#define PATH_PREFIX' "${CONFIG_FILE}" | sed 's/.*"\([^"]*\)".*/\1/')
    echo "${PATH_PREFIX_VALUE}"
else
    echo "Error: No suitable config.h file was found." >&2
    exit 1
fi
