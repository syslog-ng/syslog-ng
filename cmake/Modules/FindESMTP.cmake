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

FIND_PROGRAM(ESMTP_CONFIG libesmtp-config)

IF (ESMTP_CONFIG)
  FIND_PATH(ESMTP_INCLUDE_DIR libesmtp.h )
  EXEC_PROGRAM(${ESMTP_CONFIG} ARGS --libs OUTPUT_VARIABLE _ESMTP_LIBRARIES)
  string(REGEX REPLACE "[\r\n]" " " _ESMTP_LIBRARIES "${_ESMTP_LIBRARIES}")
  set (ESMTP_LIBRARIES ${_ESMTP_LIBRARIES} CACHE STRING "The libraries needed for ESMTP")
ENDIF (ESMTP_CONFIG)

include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(ESMTP DEFAULT_MSG ESMTP_INCLUDE_DIR ESMTP_LIBRARIES )

MARK_AS_ADVANCED(ESMTP_INCLUDE_DIR ESMTP_LIBRARIES)
