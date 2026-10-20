#############################################################################
# Copyright (c) 2026 One Identity
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

# Shadows CMake's own CONFIG-mode discovery of eclipse-paho-mqtt-c (CMAKE_MODULE_PATH
# is searched before CONFIG mode's default search) purely to add an explicit-path
# override. The delegated call forces CONFIG mode explicitly, bypassing Module-mode
# lookup entirely, so (unlike FindOpenSSL.cmake) there's no risk of recursing back
# into this file and no need to hide our own module path.

set(eclipse-paho-mqtt-c_SOURCE "AUTO" CACHE STRING "eclipse-paho-mqtt-c install prefix, or AUTO to search normally")

set(_eclipse_paho_mqtt_c_find_args eclipse-paho-mqtt-c CONFIG)
if (eclipse-paho-mqtt-c_FIND_VERSION)
    list(APPEND _eclipse_paho_mqtt_c_find_args ${eclipse-paho-mqtt-c_FIND_VERSION})
endif()
if (eclipse-paho-mqtt-c_FIND_REQUIRED)
    list(APPEND _eclipse_paho_mqtt_c_find_args REQUIRED)
endif()
if (eclipse-paho-mqtt-c_FIND_QUIETLY)
    list(APPEND _eclipse_paho_mqtt_c_find_args QUIET)
endif()
if (eclipse-paho-mqtt-c_FIND_COMPONENTS)
    list(APPEND _eclipse_paho_mqtt_c_find_args COMPONENTS ${eclipse-paho-mqtt-c_FIND_COMPONENTS})
endif()

if (NOT eclipse-paho-mqtt-c_SOURCE STREQUAL "AUTO")
    if (NOT EXISTS "${eclipse-paho-mqtt-c_SOURCE}")
        message(FATAL_ERROR "eclipse-paho-mqtt-c_SOURCE=\"${eclipse-paho-mqtt-c_SOURCE}\" is not an existing path")
    endif()
    message(STATUS "Searching for eclipse-paho-mqtt-c exclusively under eclipse-paho-mqtt-c_SOURCE=${eclipse-paho-mqtt-c_SOURCE}")
    list(APPEND _eclipse_paho_mqtt_c_find_args PATHS "${eclipse-paho-mqtt-c_SOURCE}" NO_DEFAULT_PATH)
endif()

find_package(${_eclipse_paho_mqtt_c_find_args})
unset(_eclipse_paho_mqtt_c_find_args)
