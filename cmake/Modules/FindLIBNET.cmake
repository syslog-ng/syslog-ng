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

# -----------------------------------------------------------------------------
# Find and configure LIBNET
# -----------------------------------------------------------------------------
# Variables exported:
# LIBNET_FOUND         - TRUE if libnet was found
# LIBNET_CONFIG        - path to libnet-config program
# LIBNET_INCLUDE_DIRS  - include directories needed for libnet
# LIBNET_LIBRARIES     - libraries needed to link with libnet
# LIBNET_COMPILE_DEFS  - preprocessor definitions from libnet-config
# LIBNET_CFLAGS        - original CFLAGS returned by libnet-config
# -----------------------------------------------------------------------------

find_program(LIBNET_CONFIG libnet-config)

add_library(libnet INTERFACE)

if(LIBNET_CONFIG)
  message(STATUS "Found libnet-config: ${LIBNET_CONFIG}")

  set(LIBNET_FOUND TRUE CACHE BOOL "Libnet found")

  # Get flags from libnet-config
  execute_process(
    COMMAND ${LIBNET_CONFIG} --cflags
    OUTPUT_VARIABLE _LIBNET_CFLAGS
    OUTPUT_STRIP_TRAILING_WHITESPACE
  )
  execute_process(
    COMMAND ${LIBNET_CONFIG} --libs
    OUTPUT_VARIABLE _LIBNET_LIBRARIES
    OUTPUT_STRIP_TRAILING_WHITESPACE
  )

  string(REGEX REPLACE "[\r\n]" " " _LIBNET_CFLAGS "${_LIBNET_CFLAGS}")
  string(REGEX REPLACE "[\r\n]" " " _LIBNET_LIBRARIES "${_LIBNET_LIBRARIES}")

  # This is due to libnet-config provides old fashined defines, which triggers warning on newer systems
  # for details see: https://github.com/libnet/libnet/pull/71
  set(LIBNET_CFLAGS "${LIBNET_CFLAGS} -D_DEFAULT_SOURCE")

  # Split flags into lists
  separate_arguments(_LIBNET_CFLAGS_LIST UNIX_COMMAND "${_LIBNET_CFLAGS}")

  # Separate include directories and preprocessor definitions
  set(LIBNET_INCLUDE_DIRS "")
  set(LIBNET_COMPILE_DEFS "")
  set(LIBNET_LIBRARIES ${_LIBNET_LIBRARIES})

  foreach(flag ${_LIBNET_CFLAGS_LIST})
    if(flag MATCHES "^-I(.+)")
      list(APPEND LIBNET_INCLUDE_DIRS "${CMAKE_MATCH_1}")
    elseif(flag MATCHES "^-D(.+)")
      list(APPEND LIBNET_COMPILE_DEFS "${CMAKE_MATCH_1}")
    endif()
  endforeach()

  target_include_directories(libnet INTERFACE ${LIBNET_INCLUDE_DIRS})
  target_compile_definitions(libnet INTERFACE ${LIBNET_COMPILE_DEFS})
  target_link_libraries(libnet INTERFACE ${LIBNET_LIBRARIES})

else()
  set(LIBNET_FOUND FALSE CACHE BOOL "Libnet found")
endif()

set(LIBNET_INCLUDE_DIRS "${LIBNET_INCLUDE_DIRS}" CACHE STRING "LibNet headers")
set(LIBNET_LIBRARIES "${LIBNET_LIBRARIES}" CACHE STRING "LibNet libraries")
set(LIBNET_COMPILE_DEFS "${LIBNET_COMPILE_DEFS}" CACHE STRING "LibNet compile definitions")
set(LIBNET_CFLAGS "${_LIBNET_CFLAGS}" CACHE STRING "Original cflags from libnet-config")

include(FindPackageHandleStandardArgs)

# NOTE: This will reset LIBNET_FOUND if any of the other checked vars is empty
FIND_PACKAGE_HANDLE_STANDARD_ARGS(LIBNET DEFAULT_MSG LIBNET_LIBRARIES LIBNET_INCLUDE_DIRS LIBNET_FOUND)

# Mark advanced variables for GUI
MARK_AS_ADVANCED(LIBNET_LIBRARIES LIBNET_INCLUDE_DIRS LIBNET_COMPILE_DEFS LIBNET_CFLAGS)
