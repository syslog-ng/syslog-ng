#############################################################################
# Copyright (c) 2017 Balabit
# Copyright (c) 2017 Kokan
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

# -----------------------------------------------------------------------------
# NOTE: The usage of the name external in the file names and the internal
#       variables are confusing here, as this tries to discover the internal
#       version of the given lib.
#       For legacy reasons I've kept it as it is.
#       First time I've tried to replace all the external words I found strange
#       cmake errors that led me to think the word external is a requirements
#       somwhere, but probably it was my fault only.
# TODO: Retry the renaming to internal everywhere
# -----------------------------------------------------------------------------

include(CMakeParseArguments)

function(external_or_find_package LIB_NAME)
    cmake_parse_arguments(EXTERNAL_OR_FIND_PACKAGE "REQUIRED" "" "" ${ARGN})

    set(${LIB_NAME}_SOURCE "AUTO" CACHE STRING "${LIB_NAME} library source")

    set_property(CACHE ${LIB_NAME}_SOURCE PROPERTY STRINGS internal system auto AUTO)

    # Any value other than the known keywords is treated as an explicit install prefix:
    # ${LIB_NAME}_SOURCE_PATH signals this to the matching Find${LIB_NAME}.cmake module,
    # which must search exclusively under that prefix (see libfind_pkg_detect's EXCLUSIVE_PATH).
    set(${LIB_NAME}_SOURCE_PATH "")

    # Do not rely simply on the result of the External{LIB_NAME}.cmake (e.g. the presence of the source).
    # If the user explicitly selected the system version, then do not build the internal one.
    if (NOT "${${LIB_NAME}_SOURCE}" STREQUAL "internal" AND NOT "${${LIB_NAME}_SOURCE}" STREQUAL "system" AND NOT ("${${LIB_NAME}_SOURCE}" MATCHES "^(auto|AUTO)$"))
        if (NOT EXISTS "${${LIB_NAME}_SOURCE}")
            message(FATAL_ERROR "${LIB_NAME}_SOURCE=\"${${LIB_NAME}_SOURCE}\" is neither a known keyword (internal/system/AUTO) nor an existing path")
        endif()
        set(${LIB_NAME}_SOURCE_PATH "${${LIB_NAME}_SOURCE}")
        set(${LIB_NAME}_INTERNAL FALSE)
    elseif ("${${LIB_NAME}_SOURCE}" STREQUAL "internal" OR ("${${LIB_NAME}_SOURCE}" MATCHES "^(auto|AUTO)$"))
        include(External${LIB_NAME} OPTIONAL RESULT_VARIABLE EXT_${LIB_NAME}_PATH)

        if(NOT EXT_${LIB_NAME}_PATH OR NOT EXISTS "${EXT_${LIB_NAME}_PATH}")
            set(${LIB_NAME}_INTERNAL FALSE)
        endif()
    else()
        set(${LIB_NAME}_INTERNAL FALSE)
    endif()

    if (${LIB_NAME}_INTERNAL)
       set_target_properties(${LIB_NAME} PROPERTIES EXCLUDE_FROM_ALL TRUE)
    endif()

    if (${${LIB_NAME}_INTERNAL} AND ((${${LIB_NAME}_SOURCE} STREQUAL "internal") OR ("${${LIB_NAME}_SOURCE}" MATCHES "^(auto|AUTO)$")))
        message(STATUS "Found ${LIB_NAME}: internal")
        set(${LIB_NAME}_FOUND TRUE PARENT_SCOPE)
        set(${LIB_NAME}_INCLUDE_DIR "${${LIB_NAME}_INTERNAL_INCLUDE_DIR}" CACHE STRING "${LIB_NAME} include path")
        set(${LIB_NAME}_LIBRARY "${${LIB_NAME}_INTERNAL_LIBRARY}" CACHE STRING "${LIB_NAME} library path")

    elseif (${LIB_NAME}_SOURCE_PATH OR (${${LIB_NAME}_SOURCE} STREQUAL "system") OR ("${${LIB_NAME}_SOURCE}" MATCHES "^(auto|AUTO)$"))
      if (${LIB_NAME}_SOURCE_PATH)
        message(STATUS "Searching for ${LIB_NAME} exclusively under ${LIB_NAME}_SOURCE=${${LIB_NAME}_SOURCE_PATH}")
      endif()
      if (${EXTERNAL_OR_FIND_PACKAGE_REQUIRED})
          find_package(${LIB_NAME} REQUIRED)
      else()
          find_package(${LIB_NAME} )
      endif()
      set(${LIB_NAME}_FOUND "${${LIB_NAME}_FOUND}" PARENT_SCOPE)
      unset(${LIB_NAME}_INTERNAL)
    else()
      if (${EXTERNAL_OR_FIND_PACKAGE_REQUIRED})
          message(FATAL_ERROR "Library ${LIB_NAME} is mandatory, but NOTFOUND")
      else()
          message(STATUS "Library ${LIB_NAME} is NOTFOUND")
      endif()
      unset(${LIB_NAME}_INTERNAL)
    endif()

    if (${LIB_NAME}_INTERNAL)
       set(${LIB_NAME}_INTERNAL "${${LIB_NAME}_INTERNAL}" PARENT_SCOPE)
    endif()
endfunction()

