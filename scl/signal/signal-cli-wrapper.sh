#!/bin/bash
#############################################################################
# Copyright (c) 2020 Balabit
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


for ARGUMENT in "$@"
do

  KEY=$(echo $ARGUMENT | cut -f1 -d=)
  VALUE=$(echo $ARGUMENT | cut -f2 -d=)

  case "$KEY" in
    SIGNAL_CLI_EXECUTABLE)    SIGNAL_CLI_EXECUTABLE=${VALUE} ;;
    OWN_NUMBER)               OWN_NUMBER=${VALUE} ;;
    PARTNER_NUMBER)           PARTNER_NUMBER=${VALUE} ;;
    *)
  esac

done



while read line ; do
  $SIGNAL_CLI_EXECUTABLE -u $OWN_NUMBER send $PARTNER_NUMBER -m "$line"
done
