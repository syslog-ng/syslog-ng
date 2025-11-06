# ############################################################################
# Copyright (c) 2017 Balabit
# Copyright (c) 2022 One Identity
# Copyright (c) 2025 Istvan Hoffmann <hofione@gmail.com>
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
# ############################################################################

# This is a hard dependency, so we always try to find it.
#
set(BISON_FLAGS "-Wno-other -Werror=conflicts-sr -Werror=conflicts-rr -Wcounterexamples")
set(BISON_BUILT_SOURCE_CFLAGS "-Wno-unused-but-set-variable")

find_package(BISON 3.7.6 REQUIRED)
string(REGEX MATCH "^([0-9]+)\\.([0-9]+)\\..*$" _dummy "${BISON_VERSION}")
