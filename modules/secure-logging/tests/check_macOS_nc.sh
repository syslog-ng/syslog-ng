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
# File:   check_macOS_nc.sh
# Date:   2026-05-29
#
# Test script to check whether running on macOS and
# whether the system provides netcat nc with q option

# set -x

UDP_PORT=7777
NC_HAS_Q=0
IS_MAC=0

# -- Check if nc must be provided with q option
if nc -h 2>&1 | grep -q "\-q" || nc --help 2>&1 | grep -q "\-q"; then
    NC_HAS_Q=1
fi

# -- Determine if we are on macOS
IS_MAC=0
if [ "$(uname)" = "Darwin" ]; then
    IS_MAC=1
fi

# -- Execution phase (Inside your loop) --
if [ "${NC_HAS_Q}" -eq 1 ] && [ "${IS_MAC}" -eq 0 ]; then
    # -- Use the clean -q flag for Linux/GNU systems
    #    This closes nc 3 seconds after EOF is reached.
    # nc -u -q 3 127.0.0.1 "${UDP_PORT}" <"./data.txt"
    { echo "Hello from nc with q" | nc -u -q 3 127.0.0.1 "${UDP_PORT}"; } 
else
    # -- Use the Background+Kill method for macOS (where -q is missing)
    #    or systems where -q failed detection.

    #--  Start nc in the background
    # nc -u 127.0.0.1 "${UDP_PORT}" <"./data.txt" &
    # NC_PID=$!
    { echo "Hello macOS or Linux providing nc without q" | nc -u 127.0.0.1 "${UDP_PORT}"; } &
    NC_PID=$!
    # -- Wait for the Intel VM to process the network buffer
    sleep 3
    # Kill the process to prevent the 10-minute hang
    kill -9 "${NC_PID}" 2>/dev/null || true
    wait "${NC_PID}" 2>/dev/null || true
fi

echo " "
echo "NC_HAS_Q: ${NC_HAS_Q}, IS_MAC: ${IS_MAC}"
echo " "

