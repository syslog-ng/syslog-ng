#! /bin/sh
#############################################################################
# Copyright (c) 2013 Balabit
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

top_srcdir=$(cd ${top_srcdir}; pwd)
top_builddir=$(cd ${top_builddir}; pwd)
srcdir="${top_srcdir}/tests/functional"

export top_srcdir
export top_builddir
export srcdir

install -d ${top_builddir}/tests/functional
cd ${top_builddir}/tests/functional
${PYTHON} ${top_srcdir}/tests/functional/func_test.py
