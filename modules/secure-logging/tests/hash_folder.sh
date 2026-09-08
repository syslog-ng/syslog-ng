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
# File:   hash_folder.sh
# Author: Airbus Commercial Aircraft <secure-logging@airbus.com>
# Date:   2026-05-29
#
# This script generates SHA256 checksums for all files in a specified directory,
# writing them to checksums.txt in the format:
# <relative/path/to/file> <hashsum>
#
# e.g.:
# ./sbin/syslog-ng-debun a25834e49dcd647b6929dde25155a7c5379e9ce96b160a38e233e1b4d87fba95
# ./sbin/syslog-ng 14030105c054516e7335213155f63debeab505e513e1f303b30bbf3aa82b5a88
# ./sbin/syslog-ng-ctl 0d0a6017c8b90beecbf5a002b176f4ec4b766ad915b30d427e8ec02799355f0e
#
#-----------------------------------------------------------------------

# Check if an argument (directory path) was provided
if [[ -z $1 ]]; then
    echo "Usage: $0 <path/to/folder>" >&2
    exit 1
fi

TARGET_DIR="$1"
NOW=$(date +%Y-%m-%d_%H%M%S)
OUT_FILENAME_PART=$(echo "${TARGET_DIR}" | tr / _)
OUTPUT_FILE="checksums_${OUT_FILENAME_PART}_${NOW}.txt"

# Check if the target is a directory
if [[ ! -d ${TARGET_DIR} ]]; then
    echo "Error: '${TARGET_DIR}' is not a valid directory." >&2
    exit 1
fi

# Use 'find' to locate all regular files (-type f) and pipe their full paths
# to a loop that calculates the checksum and formats the output.
# The 'cd' command is essential to calculate the relative path correctly.
(
    # Change directory to the target to get relative paths.
    # The parentheses execute this in a subshell, so the working directory
    # of the main script does not change.
    if cd "${TARGET_DIR}"; then
        # Use find to list all regular files in the current directory (which is now TARGET_DIR)
        # and below, and execute a command for each one.
        find . -type f -exec sh -c '
            # $0 is the file path relative to TARGET_DIR
            FILE_PATH="$0"
            
            # Use sha256sum on the file.
            # The output of sha256sum is: hashsum  filename
            # We use cut to extract just the hashsum.
            # Then we print the relative path followed by the hashsum.
            HASH=$(sha256sum "${FILE_PATH}" | cut -d " " -f 1)
            
            # Print the formatted line: relative/path/to/file hashsum
            printf "%s %s\\n" "${FILE_PATH}" "$HASH"
        ' {} \;
    else
        echo "Error: Could not change directory to '${TARGET_DIR}'." >&2
        exit 1
    fi
) >"${OUTPUT_FILE}"

echo " "
echo "Checksums generated successfully in ${OUTPUT_FILE}"
echo "Target directory was: ${TARGET_DIR}"
echo " "

exit 0
