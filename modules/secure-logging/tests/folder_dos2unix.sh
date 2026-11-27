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

# File:   folder_dos2unix.sh

# Helper script to convert all regular files in a folder from Windows line endings CR LF to linux line endings LF

# Usage check
if [[ -z $1 ]]; then
    echo "Usage: $0 /path/to/your/directory"
    exit 1
fi

TARGET_DIR="$1"

# Check if directory exists
if [[ ! -d ${TARGET_DIR} ]]; then
    echo "Error: Directory '${TARGET_DIR}' does not exist."
    exit 1
fi

# Check if dos2unix is installed
if ! command -v dos2unix &>/dev/null; then
    echo "Error: dos2unix is not installed. Please install it first (e.g., sudo apt install dos2unix)."
    exit 1
fi

# Find files and convert them
# We use -type f to ensure we only target files, not directories
find "${TARGET_DIR}" -type f -exec dos2unix {} +

echo "Conversion complete for files in ${TARGET_DIR}."
