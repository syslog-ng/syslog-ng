#!/bin/sh
#############################################################################
# Copyright (c) 2025 Balabit, Daniele Ferla
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

set -f

# Normalize target list (quotes/commas/spaces -> spaces)
targets=$(printf '%s' "$confgen_targets" | sed "s/[\"']//g; s/,/ /g")
set -- $targets
target_cnt=$#
i=0

printf 'channel {'
for target in $targets; do
    # Skip failover generation if failover is disabled or there are less than 2 targets
    if  [ "$confgen_failover" = "off" ] || [ "$confgen_failover" = "no" ] ||
        [ "$target_cnt" -lt 2 ]; then
        failover=""
    else
        # Automatically generate the list of failover targets if not specified
        if ! printf '%s' "$confgen_failover" | grep -qi 'servers('; then
            failover_servers=""
            for t in $targets; do
              [ "$t" = "$target" ] && continue                    # skip current target
              failover_servers="${failover_servers}, \"$t\""
            done
            failover_servers=${failover_servers#, }
            failover="failover(servers($failover_servers) $confgen_failover)"
        else
            # Use the user-specified failover configuration as-is
            failover="failover($confgen_failover)"
        fi
    fi

    printf '
    channel {
        filter {
            "%s" == "$(%% ${R_USEC} %s)"
        };
        destination {
            network("%s"
                %s
                %s
            );
        };
        flags(final);
    };\n' "$i" "$target_cnt" "$target" "$failover" "$confgen_parameters"

    i=$((i+1))
done
printf '};\n'
