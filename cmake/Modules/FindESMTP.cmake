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

# Origin: https://raw.githubusercontent.com/ckruse/cforum_cpp/master/cmake/modules/FindESMTP.cmake
# - Try to find the libesmtp library
# Once done this will define
#
#  ESMTP_FOUND - system has the libesmtp library
#  ESMTP_CONFIG
#  ESMTP_INCLUDE_DIR - the libesmtp include directory
#  ESMTP_LIBRARIES - The libraries needed to use libesmtp
#
# Based on FindPCRE.cmake
# Distributed under the BSD license.

if (ESMTP_INCLUDE_DIR AND ESMTP_LIBRARIES)
  # Already in cache, be silent
  set(ESMTP_FIND_QUIETLY TRUE)
endif (ESMTP_INCLUDE_DIR AND ESMTP_LIBRARIES)

find_package(PkgConfig)
pkg_check_modules(PC_ESMTP QUIET libesmtp-1.0)

if (PC_ESMTP_FOUND)
  find_path(ESMTP_INCLUDE_DIR NAMES libesmtp.h HINTS ${PC_ESMTP_INCLUDE_DIRS} PATH_SUFFIXES libesmtp)
  find_library(ESMTP_LIBRARIES NAMES esmtp HINTS ${PC_ESMTP_LIBRARY_DIRS})
else ()
  find_program(ESMTP_CONFIG libesmtp-config)

  if (ESMTP_CONFIG)
    find_path(ESMTP_INCLUDE_DIR libesmtp.h )
    exec_program(${ESMTP_CONFIG} ARGS --libs OUTPUT_VARIABLE _ESMTP_LIBRARIES)
    string(REGEX REPLACE "[\r\n]" " " _ESMTP_LIBRARIES "${_ESMTP_LIBRARIES}")
    set (ESMTP_LIBRARIES ${_ESMTP_LIBRARIES} CACHE STRING "The libraries needed for ESMTP")
  endif (ESMTP_CONFIG)
endif (PC_ESMTP_FOUND)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(ESMTP DEFAULT_MSG ESMTP_LIBRARIES ESMTP_INCLUDE_DIR)
mark_as_advanced(ESMTP_LIBRARIES ESMTP_INCLUDE_DIR)
