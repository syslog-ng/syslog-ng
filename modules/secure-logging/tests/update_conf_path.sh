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
# File:   update_conf_path.sh
# Date:   2026-05-29
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

if [[ $# -ne 3 ]]; then
    echo "Usage: $0 <file_path> <suffix> <add|remove>" >&2
    exit 1
fi

FILE_PATH="$1"
SUFFIX="$2"
ACTION="$3"

if [[ ! -f ${FILE_PATH} ]]; then
    echo "ERROR: File not found: ${FILE_PATH}" >&2
    exit 1
fi

# Define the two possible target paths
DATA_PATH="/tmp/test_slog/data"
DATA_SUFFIXED_PATH="/tmp/test_slog/data_${SUFFIX}"

# Decide which target path to use based on the action
if [[ ${ACTION} == "add" ]]; then
    TARGET_PATH="${DATA_SUFFIXED_PATH}"
else
    TARGET_PATH="${DATA_PATH}"
fi

# Define the full line we want to write
NEW_LINE="@define mypath \"${TARGET_PATH}\""

TMP_FILE=$(mktemp "${FILE_PATH}.XXXXXX")
trap 'rm -f "${TMP_FILE}"' EXIT

# ensure the file ends with a clean newline
[[ -n $(tail -c1 "${FILE_PATH}") ]] && echo >>"${FILE_PATH}"

sed -e 's/\r$//' -e "s|^@define mypath.*|${NEW_LINE}|" "${FILE_PATH}" >"${TMP_FILE}"
mv "${TMP_FILE}" "${FILE_PATH}"

# Final Verification: Did it actually change?
if grep -qF "${NEW_LINE}" "${FILE_PATH}"; then
    echo "SUCCESS: File '${FILE_PATH}' set to: ${NEW_LINE}"
else
    echo "ERROR: The file could not be updated." >&2
    exit 1
fi
