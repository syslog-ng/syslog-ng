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

set(ENABLE_SPOOF_SOURCE "AUTO" CACHE STRING "Enable spoof source support: ON, OFF, AUTO")
set_property(CACHE ENABLE_SPOOF_SOURCE PROPERTY STRINGS AUTO ON OFF)
message(STATUS "Checking Spoof source support")

if(ENABLE_SPOOF_SOURCE STREQUAL "OFF")
  set(SYSLOG_NG_ENABLE_SPOOF_SOURCE OFF)
  message(STATUS "  Spoof source support: disabled (forced OFF)")
  return()
endif()

find_package(LIBNET)

if("${ENABLE_SPOOF_SOURCE}" MATCHES "^(auto|AUTO)$")
  if(LIBNET_FOUND)
    set(SYSLOG_NG_ENABLE_SPOOF_SOURCE ON)
    message(STATUS "  Spoof source support: enabled (AUTO, found libnet)")
  else()
    set(SYSLOG_NG_ENABLE_SPOOF_SOURCE OFF)
    message(STATUS "  Spoof source support: disabled (AUTO, libnet not found)")
  endif()
elseif(ENABLE_SPOOF_SOURCE STREQUAL "ON")
  if(NOT LIBNET_FOUND)
    message(FATAL_ERROR "Could not find libnet, and spoof source support was explicitly enabled.")
  endif()

  set(SYSLOG_NG_ENABLE_SPOOF_SOURCE ON)
  message(STATUS "  Spoof source support: enabled (forced ON)")
else()
  message(FATAL_ERROR "Invalid value (${ENABLE_SPOOF_SOURCE}) for ENABLE_SPOOF_SOURCE (must be ON, OFF, or AUTO)")
endif()
