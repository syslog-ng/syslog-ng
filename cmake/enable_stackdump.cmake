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

set(ENABLE_STACKDUMP "AUTO" CACHE STRING "Enable stackdump using libunwind (Linux, macOS, FreeBSD only): ON, OFF, AUTO")
set_property(CACHE ENABLE_STACKDUMP PROPERTY STRINGS AUTO ON OFF)
message(STATUS "Checking stackdump support")

if(ENABLE_STACKDUMP STREQUAL "OFF")
  set(SYSLOG_NG_ENABLE_STACKDUMP OFF)
  message(STATUS "  Stackdump support: disabled (forced OFF)")
  return()
endif()

if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux" AND NOT CMAKE_SYSTEM_NAME STREQUAL "Darwin" AND NOT CMAKE_SYSTEM_NAME STREQUAL "FreeBSD")
  message(FATAL_ERROR "ENABLE_STACKDUMP is only supported on Linux, macOS, or FreeBSD.")
endif()

find_package(LIBUNWIND)

if(ENABLE_STACKDUMP STREQUAL "AUTO" OR ENABLE_STACKDUMP STREQUAL "auto")
  if(LIBUNWIND_FOUND)
    set(SYSLOG_NG_ENABLE_STACKDUMP ON)
    message(STATUS "  Stackdump support: enabled (AUTO, found libunwind)")
  else()
    set(SYSLOG_NG_ENABLE_STACKDUMP OFF)
    message(STATUS "  Stackdump support: disabled (AUTO, libunwind not found)")
  endif()
elseif(ENABLE_STACKDUMP STREQUAL "ON")
  if(NOT LIBUNWIND_FOUND)
    message(FATAL_ERROR "ENABLE_STACKDUMP was explicitly enabled, but the required libunwind library dependency could not be found")
  endif()

  set(SYSLOG_NG_ENABLE_STACKDUMP ON)
  message(STATUS "  Stackdump support: enabled (forced ON)")
endif()

if(SYSLOG_NG_ENABLE_STACKDUMP)
  set(CMAKE_REQUIRED_INCLUDES ${LIBUNWIND_INCLUDE_DIR})
  if(NOT APPLE)
    set(CMAKE_REQUIRED_LIBRARIES ${LIBUNWIND_LIBRARY})
  endif()
endif()
