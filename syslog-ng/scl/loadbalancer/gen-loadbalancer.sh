#!/bin/sh
#############################################################################
# Copyright (c) 2017 Balabit
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

TARGETS=$confgen_targets
TARGET_PARAMS=$confgen_parameters

target_cnt=$(echo $TARGETS | wc -w)
target_id=0
echo 'channel {'
for target in $TARGETS
do
#    other_targets=$(echo $TARGETS | sed -e s/\\b${target}\\b//)
    echo '  channel {'
    echo '    filter {'
    echo -n '    "'
    echo -n $target_id
    echo -n '" == "$(% ${R_MSEC} '
    echo -n $target_cnt
    echo ')"'
    echo '    };'
    echo '    destination {'
    echo -n '      network("'
    echo -n $target
    echo -n '" '
    echo -n $TARGET_PARAMS
#echo -n 'failover-servers("'
#echo -n $other_targets
#echo -n '")'
    echo '      );'
    echo '    };'
    echo '    flags(final);'
    echo '  };'
    target_id=$((target_id+1))
done
echo '};'

