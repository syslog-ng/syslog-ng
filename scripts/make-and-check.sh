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

. `dirname "$0"`/../tests/build-log-cflags-propagation.sh

main() {
  local LOG="make.log"
  echo "info: log saved to $LOG" >&2
  {
    logged_main "$@"
    echo "exit status: $?"
  } 2>&1 |
  tee "$LOG"
}

logged_main() {
  exec_prop_check "make -j V=1 install"
  S=$?
  if [ "$S" = "42" ]; then
    return $S;
  elif [ "$S" != "0" ]; then
    make V=1 --keep-going # to make error messages more readable on error
    return 1
  fi

  export CFLAGS="$CFLAGS -Werror"
  exec_prop_check "make -j 1 distcheck V=1 --keep-going" &&

  make func-test V=1 --keep-going
}

main "$@"
