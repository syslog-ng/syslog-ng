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

# FindHiredis.cmake - Try to find the Hiredis library
# Once done this will define
#
#  HIREDIS_FOUND - System has Hiredis
#  HIREDIS_INCLUDE_DIR - The Hiredis include directory
#  HIREDIS_LIBRARIES - The libraries needed to use Hiredis
#  HIREDIS_DEFINITIONS - Compiler switches required for using Hiredis


# use pkg-config to get the directories and then use these values
# in the FIND_PATH() and FIND_RARY() calls
FIND_PACKAGE(PkgConfig)
PKG_SEARCH_MODULE(PC_HIREDIS hiredis)

SET(HIREDIS_DEFINITIONS ${PC_HIREDIS_CFLAGS_OTHER})

FIND_PATH(HIREDIS_INCLUDE_DIR NAMES hiredis/hiredis.h
   HINTS
   ${PC_HIREDIS_INCLUDEDIR}
   ${PC_HIREDIS_INCLUDE_DIRS}
   PATH_SUFFIXES hiredis
   )

FIND_LIBRARY(HIREDIS_LIBRARIES NAMES hiredis
   HINTS
   ${PC_HIREDIS_DIR}
   ${PC_HIREDIS_LIBRARY_DIRS}
   )

INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(Hiredis DEFAULT_MSG HIREDIS_LIBRARIES HIREDIS_INCLUDE_DIR)

MARK_AS_ADVANCED(HIREDIS_INCLUDE_DIR HIREDIS_LIBRARIES)
