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
# TODO: Check why the autotools build does not have this.
#
set(WITH_GETTEXT "" CACHE STRING "Set the prefix where gettext is installed (e.g. /usr)")

if(WITH_GETTEXT)
  set(CMAKE_PREFIX_PATH ${WITH_GETTEXT})
  find_package(Gettext REQUIRED)
  set(CMAKE_PREFIX_PATH "")
else()
  find_package(Gettext REQUIRED)
endif()
