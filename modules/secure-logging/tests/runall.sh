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

# remove path and extension from $0
s=$0
SCRIPTNAME="$(
    b="${s##*/}"
    echo "${b%.*}"
)"
NOW=$(date +%Y-%m-%d_%H%M%S)
SUBFOLDER_TEST="test_slog"
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color (Reset)

# List your scripts in an array
scripts=("./cli11_enc_syslog_verify_n.sh"
    "./cli12v1_direct_crypt_verify_i.sh"
    "./cli12v2_base64_crypt_verify_i.sh"
    "./cli12v3_direct_ivtag_crypt_verify_i.sh"
    "./cli12v4_direct_msg_crypt_verify_i.sh"
    "./cli12v5_enc_crypt_verify_i.sh"
    "./cli13_plain_loggen_verify_n.sh"
    "./cli14_plain_loggen_file_verify_n.sh"
    "./cli15_multiline_all_logmodes_crypt_verify_n.sh"
    "./cli15_multiline_all_logmodes_crypt_verify_n_tamper.sh"
    "./cli16_plain_syslog_verify_n.sh"
    "./cli27_bin.sh"
    "./cli43_enc_syslog_verify_n_fallback_logmode.sh"
    "./cli44_crypt_verify_i_valgrind_fallback_logmode.sh"
    "./cli19_plain_valgrind.sh"
)

# exit on first error
for s in "${scripts[@]}"; do
    if ! "${s}"; then
        echo -e "${RED}ERROR${NC}: ${YELLOW}${s}${NC} failed with exit code $?"
        echo -e "${RED}FAIL${NC}"
        exit 1
    fi
done

echo " "
echo "Summary of tests:"
for s in "${scripts[@]}"; do
    echo "${s}"
done

echo " "
echo "You might want to call this script like:"
echo "$0 2>&1 | tee ${HOME}/${SUBFOLDER_TEST}/log/${SCRIPTNAME}_${NOW}.log"
echo " "
echo -e "${GREEN}SUCCESS${NC}: All scripts were executed successfully."
echo " "
