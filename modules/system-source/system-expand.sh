#! /bin/sh
#############################################################################
# Copyright (c) 2014-2016 Balabit
#
# This program is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License version 2 as published
# by the Free Software Foundation, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
#
# As an additional exemption you are allowed to compile & link against the
# OpenSSL libraries as published by the OpenSSL project. See the file
# COPYING for details.
#
#############################################################################

set -e

cfg="$(mktemp)"
out="$(mktemp)"

create_config () {
        local cfg="$1"
        cat >"${cfg}" <<EOF
@version: 3.14
@include "scl.conf"
source s_system {
# ----- Cut here -----
system();
# ----- Cut here -----
};
EOF
}

process_config () {
        local cfg="$1"
        local out="$2"

        syslog-ng -f "${cfg}" \
                  --syntax-only \
                  --preprocess-into="${out}" \
                  --no-caps \
                  >/dev/null 2>/dev/null
}

extract_system_source () {
        local out="$1"
        local IFS_SAVE="${IFS}"
        IFS="
"
        local print=0

        echo "## system() expands to:"
        echo
        for line in $(cat "${out}"); do
                case "${line}" in
                        "# ----- Cut here -----")
                                if test ${print} -eq 0; then
                                        print=1
                                else
                                        print=0
                                fi
                                ;;
                        *)
                                if test ${print} -eq 1; then
                                        echo "${line}"
                                fi
                                ;;
                esac
        done
}

cleanup () {
        local cfg="$1"
        local out="$2"

        rm -f "${cfg}" "${out}"
}

create_config "${cfg}"
process_config "${cfg}" "${out}"
extract_system_source "${out}"
cleanup "${cfg}" "${out}"
