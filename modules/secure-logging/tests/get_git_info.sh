#!/bin/sh

############################################################################
# Copyright (c) 2025 Airbus Commercial Aircraft
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

#-----------------------------------------------------------------------
# File:   get_git_info.sh
# Author: Airbus Commercial Aircraft <secure-logging@airbus.com>
# Date:   2025-12-04
#
# Scipt to run a couple of git commands to show status of git branch in
# a test.
#-----------------------------------------------------------------------

# SCRIPT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)

echo "-- The current Git commit hash"
GIT_COMMIT_HASH=$(git show -s --pretty=format:"%H - %s")
echo "${GIT_COMMIT_HASH}"
echo " "

echo "-- The current Git branch"
GIT_BRANCH=$(git rev-parse --abbrev-ref HEAD)
echo "${GIT_BRANCH}"
echo " "

echo "-- Git remote repositories"
GIT_REMOTE=$(git remote -v)
echo "${GIT_REMOTE}"
echo " "

echo "-- The current Git status"
GIT_STATUS=$(git status)
echo "${GIT_STATUS}"
echo " "
