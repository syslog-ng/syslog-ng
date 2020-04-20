#!/usr/bin/env bash
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

GUIDE_REPO=syslog-ng-ose-guides

PROJECT_DIR=$(dirname $(realpath ${BASH_SOURCE}))

set -e

echo -e "\e[33;1mDownload admin guide\e[0m"
cd $PROJECT_DIR
rm -rf $GUIDE_REPO
git clone git@github.com:balabit/${GUIDE_REPO}.git
cd $GUIDE_REPO/en
echo -e "\e[33;1mBuild profiled XML source files\e[0m"
make manpages

echo -e "\e[33;1mUpdate manpage source files\e[0m"
cd $PROJECT_DIR
rm *.xml
for new_file in $GUIDE_REPO/en/out/tmp/man/*.profiled.xml; do
    original=$(basename ${new_file/.profiled/})
    echo "Updating file '$original'"
    cp "$new_file" "./$original"
done

rm -rf $GUIDE_REPO

echo -e "\e[33;1mDONE.\e[0m"

git status
