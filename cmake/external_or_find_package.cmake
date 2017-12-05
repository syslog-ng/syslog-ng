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

include(CMakeParseArguments)

function(external_or_find_package LIB_NAME)
    cmake_parse_arguments(EXTERNAL_OR_FIND_PACKAGE "REQUIRED" "" "" ${ARGN})

    set(${LIB_NAME}_SOURCE "auto" CACHE STRING "${LIB_NAME} library source")

    set_property(CACHE ${LIB_NAME}_SOURCE PROPERTY STRINGS internal system auto)

    include(External${LIB_NAME} OPTIONAL RESULT_VARIABLE EXT_${LIB_NAME}_PATH)

    if (NOT EXISTS ${EXT_${LIB_NAME}_PATH})
        set(${LIB_NAME}_INTERNAL FALSE)
    endif()

    if  (${${LIB_NAME}_INTERNAL} AND ("internal" STREQUAL ${${LIB_NAME}_SOURCE} OR "auto" STREQUAL ${${LIB_NAME}_SOURCE} ))

        message(STATUS "Found ${LIB_NAME}: internal")
        set(${LIB_NAME}_FOUND TRUE PARENT_SCOPE)
        set(${LIB_NAME}_INCLUDE_DIR "${${LIB_NAME}_INTERNAL_INCLUDE_DIR}" CACHE STRING "${LIB_NAME} include path")
        set(${LIB_NAME}_LIBRARY "${${LIB_NAME}_INTERNAL_LIBRARY}" CACHE STRING "${LIB_NAME} library path")
        #install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/lib/ DESTINATION lib USE_SOURCE_PERMISSIONS FILES_MATCHING PATTERN "libivykis.so*")

    elseif ("system"   STREQUAL ${${LIB_NAME}_SOURCE} OR "auto" STREQUAL ${${LIB_NAME}_SOURCE})
      if (${EXTERNAL_OR_FIND_PACKAGE_REQUIRED})
          find_package(${LIB_NAME} REQUIRED)
      else()
          find_package(${LIB_NAME} )
      endif()
      set(${LIB_NAME}_FOUND "${${LIB_NAME}_FOUND}" PARENT_SCOPE)
      unset(${LIB_NAME}_INTERNAL)
    else()
      if (${EXTERNAL_OR_FIND_PACKAGE_REQUIRED})
          message(FATAL_ERROR "Library ${LIB_NAME} is mandatory but NOTFOUND")
      else()
          message(STATUS "Library ${LIB_NAME} is NOTFOUND")
      endif()
      unset(${LIB_NAME}_INTERNAL)
    endif()

    if (${LIB_NAME}_INTERNAL)
       set(${LIB_NAME}_INTERNAL "${${LIB_NAME}_INTERNAL}" PARENT_SCOPE)
       set_target_properties(${LIB_NAME} PROPERTIES EXCLUDE_FROM_ALL TRUE)
    endif()
endfunction()

