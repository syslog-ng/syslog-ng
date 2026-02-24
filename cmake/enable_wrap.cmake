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

set(ENABLE_TCP_WRAPPER "AUTO" CACHE STRING "Enable TCP wrapper support: ON, OFF, AUTO")
set_property(CACHE ENABLE_TCP_WRAPPER PROPERTY STRINGS AUTO ON OFF)
message(STATUS "Checking TCP wrapper support")

if(ENABLE_TCP_WRAPPER STREQUAL "OFF")
  set(SYSLOG_NG_ENABLE_TCP_WRAPPER OFF)
  message(STATUS "  TCP wrapper support: disabled (forced OFF)")
  return()
endif()

find_package(WRAP)

if("${ENABLE_TCP_WRAPPER}" MATCHES "^(auto|AUTO)$")
  if(WRAP_FOUND)
    set(SYSLOG_NG_ENABLE_TCP_WRAPPER ON)
    message(STATUS "  TCP wrapper support: enabled (AUTO, found libwrap)")
  else()
    set(SYSLOG_NG_ENABLE_TCP_WRAPPER OFF)
    message(STATUS "  TCP wrapper support: disabled (AUTO, libwrap not found)")
  endif()
elseif(ENABLE_TCP_WRAPPER STREQUAL "ON")
  if(NOT WRAP_FOUND)
    message(FATAL_ERROR "Could not find libwrap, and TCP wrapper support was explicitly enabled.")
  endif()

  set(SYSLOG_NG_ENABLE_TCP_WRAPPER ON)
  message(STATUS "  TCP wrapper support: enabled (forced ON)")
else()
  message(FATAL_ERROR "Invalid value (${ENABLE_TCP_WRAPPER}) for ENABLE_TCP_WRAPPER (must be ON, OFF, or AUTO)")
endif()
