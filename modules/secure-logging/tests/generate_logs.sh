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
# File:   generate_logs.sh
# Date:   2026-05-29
#
# generate_logs_fast.sh - Generates random RFC5424-compliant log lines
#                         with a sequential log entry number.
#
# This is an optimized POSIX-compliant script for speed.
#
# Usage: ./generate_logs.sh <number_of_lines>
#

# --- Argument Validation ---

if [ "$#" -ne 1 ]; then
    echo "Usage: $0 <number_of_lines>" >&2
    exit 1
fi

case "$1" in
*[!0-9]* | "" | 0)
    echo "Error: Argument must be a positive integer." >&2
    exit 1
    ;;
*)
    NUM_LINES="$1"
    ;;
esac

# --- Configuration ---

# RFC5424 Header fields
PRI="<13>"
VERSION="1"
APP_NAME="Rfc5424Gen"
MSG_ID="TestMsg"
STRUCTURED_DATA="-"

# Max line length (bytes), including the final newline
MAX_LINE_BYTES=2044

# Range for random *number of words* (not bytes)
MIN_WORDS=20
MAX_WORDS=250 # NOTE: You may need to lower this manually!

# Pool of words/characters to build the random message
WORD_POOL="log message test info warning error debug critical äöüÄÖÜß français español àèìòù áéíóúý âêîôû ãñõ ÀÈÌÒÙ ÁÉÍÓÚÝ ÂÊÎÔÛ ÃÑÕ Çç ¡¿ 😀 🚀 🔥 💻 ✨ 🌍 🔒 🔑 💡 🔔"

# --- Get static info ONCE ---
HOSTNAME=$(uname -n)
PROCID=$$
TIMESTAMP=$(date -u +'%Y-%m-%dT%H:%M:%SZ')

# --- Build static header ONCE ---
HEADER="${PRI}${VERSION} ${TIMESTAMP} ${HOSTNAME} ${APP_NAME} ${PROCID} ${MSG_ID} ${STRUCTURED_DATA}"

# Calculate max bytes for the line content (total - 1 for newline)
MAX_CONTENT_BYTES=$((MAX_LINE_BYTES - 1)) # This will be 2043

# --- Main processing pipe ---

# 1. Use ONE awk process to generate ALL random message payloads.
awk -v num_lines="${NUM_LINES}" \
    -v min_words="${MIN_WORDS}" \
    -v max_words="${MAX_WORDS}" \
    -v word_pool="${WORD_POOL}" '
BEGIN {
    srand() # Seed RNG once

    # Split the word pool into an array for fast lookup
    num_words = split(word_pool, words, " ")

    # Main loop (inside awk, very fast)
    for (i = 1; i <= num_lines; i++) {

        # Get random number of words for this line
        rand_word_count = int(rand() * (max_words - min_words + 1)) + min_words

        # --- MODIFIED: Start with an empty message ---
        msg = ""

        # Word-building loop
        for (j = 1; j <= rand_word_count; j++) {
            # Get random word index (1-based)
            rand_idx = int(rand() * num_words) + 1
            msg = msg words[rand_idx] " "
        }

        # --- MODIFIED: Print number and message separated by a colon ---
        # We send the line number *separately* from the payload
        print i ":" msg
    }
}
' |
    # 2. Pipe the awk output to a simple shell 'while read' loop.
    # --- MODIFIED: Use IFS=: to split on the colon ---
    while IFS=: read -r LINE_NUM MSG_PAYLOAD; do

        # Re-assemble the *full* message content
        MSG_CONTENT="log entry ${LINE_NUM} ${MSG_PAYLOAD}"

        # Assemble the full line
        FULL_LINE="${HEADER} ${MSG_CONTENT}"

        # Get current line content byte length (UTF-8 safe)
        CURRENT_LINE_BYTES=$(printf %s "${FULL_LINE}" | wc -c)

        # --- MODIFIED BLOCK ---
        # Check if line content is within the limit.
        if [ "${CURRENT_LINE_BYTES}" -le "${MAX_CONTENT_BYTES}" ]; then

            # If it's safe, print the real line.
            echo "${FULL_LINE}"

        else
            # --- NEW: Line is too long, print a dummy line ---
            # We use the LINE_NUM we read to keep the sequence correct.
            DUMMY_MSG="log entry ${LINE_NUM} [Line truncated: content exceeded limit]"
            echo "${HEADER} ${DUMMY_MSG}"
        fi

    done

exit 0
