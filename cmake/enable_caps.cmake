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

set(ENABLE_LINUX_CAPS "AUTO" CACHE STRING "Enable Linux capabilities: ON, OFF, AUTO")
set_property(CACHE ENABLE_LINUX_CAPS PROPERTY STRINGS AUTO ON OFF)
message(STATUS "Checking Linux capabilities support")

if(ENABLE_LINUX_CAPS STREQUAL "OFF")
  set(SYSLOG_NG_ENABLE_LINUX_CAPS OFF)
  message(STATUS "  Linux capabilities: disabled (forced OFF)")
  return()
endif()

if(ENABLE_LINUX_CAPS STREQUAL "ON" AND NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
  message(FATAL_ERROR "  Linux capabilities can only be enabled on Linux systems.")
endif()

find_package(LIBCAP)

if("${ENABLE_LINUX_CAPS}" MATCHES "^(auto|AUTO)$")
  if(PC_LIBCAP_FOUND)
    set(SYSLOG_NG_ENABLE_LINUX_CAPS ON)
    message(STATUS "  Linux capabilities support: enabled (AUTO, found libcap)")
  else()
    set(SYSLOG_NG_ENABLE_LINUX_CAPS OFF)
    message(STATUS "  Linux capabilities support: disabled (AUTO, libcap not found)")
  endif()
elseif(ENABLE_LINUX_CAPS STREQUAL "ON")
  if(NOT PC_LIBCAP_FOUND)
    message(FATAL_ERROR "Could not find libcap, and Linux capabilities were explicitly enabled.")
  endif()

  set(SYSLOG_NG_ENABLE_LINUX_CAPS ON)
  message(STATUS "  Linux capabilities: enabled (forced ON)")
else()
  message(FATAL_ERROR "Invalid value (${ENABLE_LINUX_CAPS}) for ENABLE_LINUX_CAPS (must be ON, OFF, or AUTO)")
endif()
