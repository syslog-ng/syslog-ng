#!/bin/sh
#############################################################################
# Copyright (c) 2017 Balabit
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
set -e

BASE_URL="https://syslog-ng.github.io/manpages/"

FILES="
  dqtool.1
  loggen.1
  pdbtool.1
  persist-tool.1
  secure-logging.7
  slogencrypt.1
  slogkey.1
  slogverify.1
  syslog-ng.8
  syslog-ng.conf.5
  syslog-ng-ctl.1
  syslog-ng-debun.1
"

usage() {
  echo "Usage: $0 -f OUTPUT_DIR"
  echo "       $0 -u SYSCONFDIR PREFIX OUTPUT_DIR"
  echo ""
  echo "  -f OUTPUT_DIR   Fetch manpages from ${BASE_URL} and store them in OUTPUT_DIR"
  echo "  -u SYSCONFDIR PREFIX OUTPUT_DIR   Update hardcoded paths in manpages located in OUTPUT_DIR"
  echo ""
  exit 1
}

echo "Fetch or update syslog-ng manpages."

if [ "$#" -lt 2 ]; then
  usage
fi

TODO=$1
if [ "$TODO" = "-f" ]; then
  if [ "$#" -ne 2 ]; then
    usage
  fi
  OUTPUT_DIR=$2
elif [ "$TODO" = "-u" ]; then
  if [ "$#" -ne 4 ]; then
    usage
  fi
  SYSCONFDIR=$2
  PREFIX=$3
  OUTPUT_DIR=$4
else
  usage
fi

mkdir -p "$OUTPUT_DIR" || true

for FILE in $FILES; do
  if [ "$TODO" = "-f" ]; then
    echo "Downloading $FILE ..."
    curl -fsSL "$BASE_URL/$FILE" -o "$OUTPUT_DIR/$FILE"
  else
    echo "Updating $FILE ..."
    sed -e "s,/opt/syslog\\\*-ng/etc,$SYSCONFDIR,g" \
        -e "s,/opt/syslog\\\*-ng/,$PREFIX/,g" \
        <"$OUTPUT_DIR/$FILE" >"$OUTPUT_DIR/$FILE.tmp" \
        && mv "$OUTPUT_DIR/$FILE.tmp" "$OUTPUT_DIR/$FILE"
  fi
done

echo "DONE."
