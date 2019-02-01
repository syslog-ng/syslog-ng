#!/bin/sh
#############################################################################
# Copyright (c) 2016 Balabit
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

exec_prop_check() {
  local ERROREXIT="42"
  local CMD="$1"
  local BUILDLOG="build.$$.log"
  {
    eval "$CMD"
    echo $?
  } |
  tee "$BUILDLOG" |
  reduce_verbosity

  S=`tail -n 1 "$BUILDLOG"`
  if [ "$S" -eq 0 ]; then
    build_log_cflags_propagation "$BUILDLOG"
    S=$?
    rm "$BUILDLOG"
    if [ $S -ne 0 ]; then
      return $ERROREXIT
    fi
  else
    rm "$BUILDLOG"
    return $S
  fi
}

####
# private functions:

reduce_verbosity() {
  grep --line-buffered --invert-match --extended-regexp "^(\
libtool: (link|relink|install): |\
depbase=|\
(test -z|rm|\./lib/merge-grammar.py|\./doc/mallard2man\.py) |\
`printf "\t"`?/bin/bash |\
`printf "\t"`?(gcc|mv) \
)"
}

build_log_cflags_propagation() {
  local BUILDLOG="$1"
  local NOTPROP="`find_not_prop "$BUILDLOG"`"
  if [ -z "$NOTPROP" ]; then
    echo "info: CFLAGS propagation test passed" >&2
    true
  else
    local N=`echo "$NOTPROP" | wc -l`
    echo "error: -Wshadow/-Werror did not propagate via CFLAGS in $N cases:" >&2

    printf "%s\n" "$NOTPROP" |
    sed "s~^~> ~" >&2
    false
  fi
}

find_not_prop() {
  {
    find_gcc "$@" |
    ignore_submodule_gcc |
    grep -vE -- " -Wshadow( |.* )-Werror "
  } 2>&1
}

find_gcc() {
  grep -E "^(\
libtool: compile: +gcc |\
`printf "\t"`?/bin/bash \./libtool  --tag=CC   --mode=compile gcc |\
`printf "\t"`?gcc \
)" "$@"
}

ignore_submodule_gcc() {
  ignore_submodule_gcc_ivykis
}

ignore_submodule_gcc_ivykis() {
  grep -vE -- "\<(\
gcc -DHAVE_CONFIG_H -I\. -I\.\./\.\. +-D_GNU_SOURCE -I\.\./\.\./src/include -I\.\./\.\./src/include |\
gcc -DHAVE_CONFIG_H -I\. -I\.\./\.\.  -I\.\./\.\./src/include -I\.\./\.\./src/include |\
gcc -DHAVE_CONFIG_H -I\. -I\.\. +-D_GNU_SOURCE -I\.\./src/include -I\.\./src/include |\
gcc( -std=gnu99)? -DHAVE_CONFIG_H -I\. -I\.\./\.\./\.\./\.\./lib/ivykis/src |\
gcc( -std=gnu99)? -DHAVE_CONFIG_H -I\. -I\.\./\.\./\.\./\.\./lib/ivykis/test(.mt)? |\
gcc( -std=gnu99)? -DHAVE_CONFIG_H -I\. -I\.\./\.\./\.\./\.\./\.\./lib/ivykis/contrib/iv_getaddrinfo |\
gcc -std=gnu99 -DHAVE_CONFIG_H -I\. -I\.\./\.\./\.\./\.\./\.\./lib/ivykis/contrib/kojines \
)" "$@"
}
