#############################################################################
# Copyright (c) 2016 Balabit
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

# Origin: https://github.com/SirCmpwn/sway/blob/master/CMake/FindJsonC.cmake
# - Find json-c
# Find the json-c libraries
#
#  This module defines the following variables:
#     JSONC_FOUND        - True if JSONC is found
#     JSONC_LIBRARY      - JSONC libraries
#     JSONC_INCLUDE_DIR  - JSONC include directories
#

find_package(PkgConfig)
pkg_check_modules(PC_JSONC QUIET JSONC)
find_path(JSONC_INCLUDE_DIR NAMES json.h HINTS ${PC_JSONC_INCLUDE_DIRS} PATH_SUFFIXES json-c json)
find_library(JSONC_LIBRARY  NAMES json-c HINTS ${PC_JSONC_LIBRARY_DIRS})

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(JSONC DEFAULT_MSG JSONC_LIBRARY JSONC_INCLUDE_DIR)
mark_as_advanced(JSONC_LIBRARY JSONC_INCLUDE_DIR)
