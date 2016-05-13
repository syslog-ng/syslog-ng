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

# Origin: https://github.com/hhetter/smbtatools/blob/master/FindLibDBI.cmake
# Try to find libdbi
# Once done this will define:
# LIBDBI_FOUND - System has Libdbi
# LIBDBI_INCLUDE_DIRS - libdbi include directories
# LIBDBI_LIBRARIES - libraries need to use libdbi
# LIBDBI_DEFINITIONS - compiler switches for libdbi
# 2011 by Holger Hetterich <ozzy@metal-district.de>
#

find_package(PkgConfig)
pkg_check_modules(PC_LIBDBI QUIET libdbi)
set(LIBDBI_DEFINITIONS ${PC_LIBDBI_CFLAGS_OTHER})

find_path(LIBDBI_INCLUDE_DIR dbi.h
    HINTS ${PC_LIBDBI_INCLUDE_DIR} ${PC_LIBDBI_INCLUDE_DIRS}
    PATH_SUFFIXES dbi )

find_library(LIBDBI_LIBRARY NAMES dbi libdbi
    HINTS ${PC_LIBDBI_LIBDIR} ${PC_LIBDBI_LIBRARY_DIRS})

set(LIBDBI_LIBRARIES ${LIBDBI_LIBRARY} )
set(LIBDBI_INCLUDE_DIRS ${LIBDBI_INCLUDE_DIR})

include(FindPackageHandleStandardArgs)
# handle the QUIETLY and REQUIRES arguments and set LIBDBI_FOUND to TRUE
# if all listed variables are TRUE
find_package_handle_standard_args(LibDbi DEFAULT_MSG
    LIBDBI_LIBRARY LIBDBI_INCLUDE_DIR)
mark_as_advanced(LIBDBI_INCLUDE_DIR LIBDBI_LIBRARY)
