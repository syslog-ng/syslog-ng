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

# Find wrap library
#
# Released under BSD license
#
#  WRAP_INCLUDE_DIRS - where to find tcpd.h, etc
#  WRAP_LIBRARIES    - Lists of libraries when using libwrap
#  WRAP_FOUND        - True if wrap found

INCLUDE(FindPackageHandleStandardArgs)

# Look for the header file
FIND_PATH(WRAP_INCLUDE_DIR NAMES tcpd.h)
MARK_AS_ADVANCED(WRAP_INCLUDE_DIR)

# Look for the library
SET(WRAP_LIBS wrap)
FIND_LIBRARY(WRAP_LIBRARY NAMES ${WRAP_LIBS})
MARK_AS_ADVANCED(WRAP_LIBRARY)

FIND_PACKAGE_HANDLE_STANDARD_ARGS(WRAP REQUIRED_VARS WRAP_LIBRARY WRAP_INCLUDE_DIR)

# Copy the result to output variables
IF(WRAP_FOUND)
  SET(WRAP_LIBRARIES ${WRAP_LIBRARY})
  SET(WRAP_INCLUDE_DIRS ${WRAP_INCLUDE_DIR})
ELSE(WRAP_FOUND)
  SET(WRAP_LIBS)
  SET(WRAP_LIBRARY)
  SET(WRAP_LIBRARIES)
  SET(WRAP_INCLUDE_DIR)
  SET(WRAP_INCLUDE_DIRS)
ENDIF(WRAP_FOUND)
