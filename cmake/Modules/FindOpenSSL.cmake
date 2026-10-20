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

# Shadows CMake's own FindOpenSSL.cmake (CMAKE_MODULE_PATH is searched before
# CMake's bundled Modules/) purely to add OPENSSL_SOURCE support, then
# delegates the actual detection back to the stock module.

set(OPENSSL_SOURCE "AUTO" CACHE STRING "OpenSSL install prefix, or AUTO to search normally")

if (NOT OPENSSL_SOURCE STREQUAL "AUTO")
    if (NOT EXISTS "${OPENSSL_SOURCE}")
        message(FATAL_ERROR "OPENSSL_SOURCE=\"${OPENSSL_SOURCE}\" is not an existing path")
    endif()
    message(STATUS "Searching for OpenSSL exclusively under OPENSSL_SOURCE=${OPENSSL_SOURCE}")
    # the stock module can still silently fall back to its own default search
    # if this hint doesn't pan out, hence the post-resolution check below
    set(OPENSSL_ROOT_DIR "${OPENSSL_SOURCE}")
endif()

# hide ourselves so the nested find_package() below resolves to CMake's
# bundled module instead of recursing back into this file
set(_openssl_module_path_save "${CMAKE_MODULE_PATH}")
list(REMOVE_ITEM CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}")

# re-forward the FIND_* state the outer find_package(OpenSSL) already set,
# since a bare nested call would otherwise reset REQUIRED/version/components
set(_openssl_find_args OpenSSL)
if (OpenSSL_FIND_VERSION)
    list(APPEND _openssl_find_args ${OpenSSL_FIND_VERSION})
endif()
if (OpenSSL_FIND_REQUIRED)
    list(APPEND _openssl_find_args REQUIRED)
endif()
if (OpenSSL_FIND_QUIETLY)
    list(APPEND _openssl_find_args QUIET)
endif()
if (OpenSSL_FIND_COMPONENTS)
    list(APPEND _openssl_find_args COMPONENTS ${OpenSSL_FIND_COMPONENTS})
endif()

find_package(${_openssl_find_args})

set(CMAKE_MODULE_PATH "${_openssl_module_path_save}")
unset(_openssl_module_path_save)
unset(_openssl_find_args)

if (NOT OPENSSL_SOURCE STREQUAL "AUTO" AND OPENSSL_FOUND)
    get_filename_component(_openssl_source_include_real "${OPENSSL_SOURCE}/include" REALPATH)
    get_filename_component(_openssl_resolved_include_real "${OPENSSL_INCLUDE_DIR}" REALPATH)
    if (NOT _openssl_resolved_include_real STREQUAL _openssl_source_include_real)
        message(FATAL_ERROR "OPENSSL_SOURCE=${OPENSSL_SOURCE} was requested, but the OpenSSL actually resolved (include dir: ${OPENSSL_INCLUDE_DIR}) does not come from there")
    endif()
    unset(_openssl_source_include_real)
    unset(_openssl_resolved_include_real)
endif()

# FindOpenSSL.cmake ends here
