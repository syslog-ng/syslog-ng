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
# File:   update_conf_path.sh
# Date:   2025-12-09
#
# Helper script to change the test path from within another script
# Note: Testscripts might call this this script to change their
# temporary test path.
#
# The base path is always /tmp/test_slog/data
# @define mypath "/tmp/test_slog/data"
# add adds _<suffix > to it
# remove removes _<suffix> from it
#
# <script> <path> <suffix> add
# <script> <path> <suffix> remove
#
# Example:
# ./update_conf_path.sh ./syslog-ng-test-udp-nc.conf "frank_zappa_1978" add
# ./update_conf_path.sh ./syslog-ng-test-udp-nc.conf "frank_zappa_1978" remove

set -x

if [ "$#" -ne 3 ]; then
    echo "Usage: $0 <file_path> <suffix_string> <add|remove>" >&2
    exit 1
fi

FILE_PATH="$1"
SUFFIX="$2"
ACTION="$3"

if [ ! -f "${FILE_PATH}" ]; then
    echo "ERROR: File not found: ${FILE_PATH}" >&2
    exit 1
fi

BASE_LINE='@define mypath "/tmp/test_slog/data"'
SUFFIXED_LINE="@define mypath \"/tmp/test_slog/data_${SUFFIX}\""

case "${ACTION}" in
add)
    SEARCH_LINE="${BASE_LINE}"
    REPLACE_LINE="${SUFFIXED_LINE}"
    ;;
remove)
    SEARCH_LINE="${SUFFIXED_LINE}"
    REPLACE_LINE="${BASE_LINE}"
    ;;
*)
    echo "ERROR: Invalid action '${ACTION}'. Must be 'add' or 'remove'." >&2
    exit 1
    ;;
esac

sed -i 's#^'"${SEARCH_LINE}"'$#'"${REPLACE_LINE}"'#' "${FILE_PATH}"
if [ $? -eq 0 ]; then
    echo "File '${FILE_PATH}' updated successfully."
else
    echo "DEBUG: sed -i 's#^'${SEARCH_LINE}'$#'${REPLACE_LINE}'#' \"${FILE_PATH}\"" >&2
    echo "Error: Failed to update '${FILE_PATH}'." >&2
    exit 1
fi
