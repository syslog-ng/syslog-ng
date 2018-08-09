#############################################################################
# Copyright (c) 2018 Balabit
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

include(LibFindMacros)
include(FindPackageHandleStandardArgs)

find_package(PkgConfig)

pkg_check_modules(PC_LIBCAP libcap QUIET)
find_path(LIBCAP_INCLUDE_DIR NAMES sys/capability.h HINTS ${PC_LIBCAP_INCLUDE_DIRS})
find_library(LIBCAP_LIBRARY  NAMES cap              HINTS ${PC_LIBCAP_LIBRARY_DIRS})

find_package_handle_standard_args(LIBCAP DEFAULT_MSG LIBCAP_LIBRARY LIBCAP_INCLUDE_DIR)

