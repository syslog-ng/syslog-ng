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
# File:   loop_cli_x.sh
# Date:   2026-05-29

set -x
set -o pipefail

MAX_LOOP=33
#SCRIPT="./cli12v1_direct_crypt_verify_i.sh"
#SCRIPT="./cli12v2_base64_crypt_verify_i.sh"
#SCRIPT="./cli12v3_direct_ivtag_crypt_verify_i.sh"
SCRIPT="./cli12v4_direct_msg_crypt_verify_i.sh"
#SCRIPT="./cli12v5_enc_crypt_verify_i.sh"

# error counter, success when this script returns 0
cnt_error=0

i=0
while [[ ${i} -lt ${MAX_LOOP} ]]; do
    i=$((i + 1))
    echo "i: ${i} of ${MAX_LOOP}"

    if ! "${SCRIPT}"; then
        echo "Script ${SCRIPT} returns with error!"
        cnt_error=$((cnt_error + 1))
    fi

    if ! [[ ${cnt_error} -eq 0 ]]; then
        break
    fi
done

echo "return cnt_error: ${cnt_error}"
if [[ ${cnt_error} -eq 0 ]]; then
    echo "PASS"
else
    echo "Found ERROR"
    echo "FAIL"
fi

echo " "
echo Done
echo " "
exit "${cnt_error}"
