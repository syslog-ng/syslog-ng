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
# File:   tamper_file.sh
# Date:   2026-05-29
#
# Script to corrupt a file by toggling some bits
#
## Utility Functions

#-----------------------------------------------------------------------
# Get file size in bytes

get_file_size() {
    _size_with_space=$(wc -c <"$1")
    #-- Use 'echo' and 'tr' to remove all whitespace, ensuring only the number remains.
    echo "${_size_with_space}" | tr -d ' '
}

#-----------------------------------------------------------------------
# Generate a pseudo-random number between min (inclusive) and max (inclusive)
# Relies on the non-POSIX, but common, shell variable $RANDOM.
# Usage: prng_range <min> <max>

prng_range() {
    _min="$1"
    _max="$2"

    # Calculate the size of the range
    _range=$((_max - _min + 1))

    # Check if ${RANDOM} is available; exit if not (strict POSIX shells like dash)
    if [[ -z ${RANDOM} ]]; then
        echo "Error: ${RANDOM} is not available. Please use a shell like Bash or Ksh." >&2
        return 1
    fi

    # Calculate the random number using modulus
    echo $((RANDOM % _range + _min))
}

#-----------------------------------------------------------------------
# Corrupt File Function
# Arguments:
#   $1: path to the binary file
#   $2: number of bits to change (1 to 10)

corrupt_file() {
    FILE_PATH="$1"
    BITS_TO_FLIP="$2"

    if [[ ! -f ${FILE_PATH} ]]; then
        echo "Error: File not found at ${FILE_PATH}" >&2
        return 1
    fi

    # Using shell arithmetic for checks (requires Bash/Ksh/Zsh)
    if [[ ${BITS_TO_FLIP} -lt 1 ]] || [[ ${BITS_TO_FLIP} -gt 10 ]]; then
        echo "Error: Number of bits to change must be between 1 and 10." >&2
        return 1
    fi

    FILE_SIZE_BYTES=$(get_file_size "${FILE_PATH}")
    FILE_SIZE_BITS=$((FILE_SIZE_BYTES * 8))

    if [[ ${FILE_SIZE_BYTES} -eq 0 ]]; then
        echo "Error: File is empty." >&2
        return 1
    fi

    echo "Target file: ${FILE_PATH} (${FILE_SIZE_BYTES} bytes)"
    echo "Flipping ${BITS_TO_FLIP} pseudo-random bits..."

    # Loop to flip the requested number of bits
    i=0
    while [[ ${i} -lt ${BITS_TO_FLIP} ]]; do

        # Select a random bit position (0 to FILE_SIZE_BITS - 1)
        MAX_BIT_POS=$((FILE_SIZE_BITS - 1))
        BIT_POS=$(prng_range 0 "${MAX_BIT_POS}")

        # Calculate the **byte offset** and the **bit-within-byte offset**
        BYTE_OFFSET=$((BIT_POS / 8))
        BIT_IN_BYTE=$((BIT_POS % 8))

        echo "  - Flipping bit $((i + 1)): Global bit ${BIT_POS} (Byte ${BYTE_OFFSET}, Bit ${BIT_IN_BYTE})"

        # 3. Read the single byte at the offset
        # Use dd and od to read the byte as hex
        # CURRENT_BYTE_HEX=$(dd if="${FILE_PATH}" bs=1 skip="${BYTE_OFFSET}" count=1 2>/dev/null | od -An -t x1)
        # CURRENT_BYTE_HEX=$(echo "${CURRENT_BYTE_HEX}" | tr -d '[:space:]')

        # --- FIX FOR SC2312 ---
        # Use a temporary file to store the byte read by dd
        TEMP_BYTE_FILE="/tmp/byte_read_$$"

        # Read the byte into the temp file. dd's exit status is now explicit.
        if ! dd if="${FILE_PATH}" of="${TEMP_BYTE_FILE}" bs=1 skip="${BYTE_OFFSET}" count=1 2>/dev/null; then
            echo "Warning: dd failed to read byte at offset ${BYTE_OFFSET}. Skipping this flip." >&2
            rm -f "${TEMP_BYTE_FILE}" # Clean up
            continue                  # Move to the next iteration
        fi

        # Process the temporary file separately with od
        CURRENT_BYTE_HEX=$(od -An -t x1 "${TEMP_BYTE_FILE}")

        # Remove the temporary file
        rm -f "${TEMP_BYTE_FILE}"

        # Clean up whitespace from od output
        CURRENT_BYTE_HEX=$(echo "${CURRENT_BYTE_HEX}" | tr -d '[:space:]')
        # ----------------------

        # Convert the hex byte to an integer
        CURRENT_BYTE_DEC=$((16#"${CURRENT_BYTE_HEX}"))

        # Calculate the **XOR mask** (1, 2, 4, 8, 16, 32, 64, 128)
        XOR_MASK=$((1 << "${BIT_IN_BYTE}"))

        # Apply the **bit flip** (XOR operation)
        NEW_BYTE_DEC=$(("${CURRENT_BYTE_DEC}" ^ "${XOR_MASK}"))

        # Convert the new byte value (decimal) back to a single byte character
        # Format as a two-digit hex string
        NEW_BYTE_HEX=$(printf "%02x" "${NEW_BYTE_DEC}")

        # Write the new byte back to the file using dd
        # FINDME TODO TEST
        #        printf '\x%s' "${NEW_BYTE_HEX}" | dd of="${FILE_PATH}" bs=1 seek="${BYTE_OFFSET}" count=1 conv=notrunc 2>/dev/null
        echo -n "${NEW_BYTE_HEX}" | xxd -r -p | dd of="${FILE_PATH}" bs=1 seek="${BYTE_OFFSET}" count=1 conv=notrunc 2>/dev/null

        i=$((i + 1))
    done

    echo "File corruption complete."
}

MAX_BITS_TO_FLIP=10

# --- WRAPPER / MAIN EXECUTION ---

usage() {
    echo "Usage: $0 <file_path> <bits_to_flip>"
    echo ""
    echo "  <file_path>     Path to the binary file to manipulate."
    echo "  <bits_to_flip>  Number of bits to randomly flip (integer from 1 to ${MAX_BITS_TO_FLIP})."
    exit 1
}

#-- Check for required number of arguments
if [[ $# -ne 2 ]]; then
    usage
fi

FILE="$1"
COUNT="$2"

#-- Check if the file exists and is readable
if [[ ! -f ${FILE} ]]; then
    echo "Error: File not found or is not a regular file: ${FILE}" >&2
    exit 1
fi
if [[ ! -r ${FILE} ]] || [[ ! -w ${FILE} ]]; then
    echo "Error: File must be readable and writable: ${FILE}" >&2
    exit 1
fi

#-- Check if the second argument (count) is a valid integer in range
# Check if it contains only digits (simple check)
if ! printf "%d" "${COUNT}" >/dev/null 2>&1; then
    echo "Error: Second argument must be a valid integer." >&2
    usage
fi

#-- Check range
if [[ ${COUNT} -lt 1 ]] || [[ ${COUNT} -gt ${MAX_BITS_TO_FLIP} ]]; then
    echo "Error: Number of bits to flip must be between 1 and ${MAX_BITS_TO_FLIP}." >&2
    usage
fi

#-- Backup of the file that is tampered
NOW=$(date +%Y-%m-%d_%H%M%S)
PATH_OF_FILE=$(readlink -f "${FILE}")
DIRECTORY=$(dirname "${PATH_OF_FILE}")
SHORTFILENAME=$(basename "${FILE}")
BACKUPFILE=${DIRECTORY}/${SHORTFILENAME}.${NOW}.bak
echo "Backup original file as ${BACKUPFILE}"
cp "${FILE}" "${BACKUPFILE}"
if [[ ! -f ${BACKUPFILE} ]]; then
    echo "Failed to backup file! Original won't be tampered! Exit"
    exit 1
fi

#-- Call the core function
corrupt_file "${FILE}" "${COUNT}"
