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

# Function: tamper_base64
# Description: Decodes a Base64 string, flips exactly one byte to a
#              guaranteed different value, and re-encodes it.
# Usage:
# tdata_b64=$(./tamper_b64.sh "${original_b64}")

tamper_base64() {
    local input="$1"

    # Convert Base64 to a Hex string in memory
    # We use xxd -p for a plain hex dump and -c 0 to keep it on one line
    local hex_string
    hex_string=$(echo -n "${input}" | base64 -d 2>/dev/null | xxd -p -c 0)

    # If decoding fails or input is empty, return original
    if [[ -z ${hex_string} ]]; then
        echo "${input}"
        return 1
    fi

    # Calculate byte positions (2 hex chars = 1 byte)
    local num_bytes=$((${#hex_string} / 2))
    local rand_byte_pos=$((RANDOM % num_bytes))
    local char_pos=$((rand_byte_pos * 2))

    # Extract the old byte and ensure the new one is different
    local old_hex="${hex_string:char_pos:2}"
    local old_val=$((16#${old_hex}))

    # Add a random offset between 1-255 to guarantee mutation
    local offset=$(((RANDOM % 255) + 1))
    local new_val=$(((old_val + offset) % 256))
    local new_hex
    new_hex=$(printf "%02x" "${new_val}")

    # Splice and Re-encode
    local tampered_hex="${hex_string:0:char_pos}${new_hex}${hex_string:char_pos+2}"
    echo -n "${tampered_hex}" | xxd -r -p | base64 -w 0
}

# --- Script Execution Logic ---
# This block allows the script to be used as a standalone command.
# If the script is being executed directly (not sourced), run the function.
if [[ ${BASH_SOURCE[0]} == "${0}" ]]; then
    if [[ -z $1 ]]; then
        echo "Usage: $0 <base64_string>" >&2
        exit 1
    fi

    if command -v xxd &>/dev/null; then
        echo "xxd is available."
    else
        echo "xxd is not available."
        exit 1
    fi

    if command -v base64 &>/dev/null; then
        echo "base64 is available."
    else
        echo "base64 is not available."
        exit 1
    fi

    tamper_base64 "$1"
    echo "" # Add a newline for terminal readability
fi
