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

# Expose useful variables:
#   CXX_STANDARD_FOUND        (TRUE/FALSE)
#   CXX_STANDARD_USED         (14 or 17)
#   CXX_STANDARD_OPTION       (the compiler flag string CMake knows about, if any)
#   SYSLOG_NG_ENABLE_CPP      (ON/OFF)
# ------------------------------------------------
#
# This module enables C++ support
# It mimics the behavior of Autotools' AX_CXX_COMPILE_STDCXX([14] ... then [17])
# likewe have in configure.ac, but it also adds a layer of ENABLE_CPP
#
# NOTE: Keep in sync with configure.ac !!!
#

if(NOT DEFINED ENABLE_CPP)
  set(AUTO_ENABLE_CPP ON INTERNAL)
else()
  set(AUTO_ENABLE_CPP OFF INTERNAL)
endif()

option(ENABLE_CPP "Enable C++" ON)
message(STATUS "Checking C++ support")

if(NOT ENABLE_CPP)
  set(SYSLOG_NG_ENABLE_CPP OFF)
  message(STATUS "  C++ support: disabled (forced OFF)")
  return()
endif()

enable_language(CXX)

# Prefer C++14, allow upgrade to C++17
# try mirror AX_CXX_COMPILE_STDCXX([14] and [17] if available))
set(CXX_STANDARDS 17 14)
set(CXX_STANDARD_FOUND FALSE)
set(CXX_STANDARD_USED "")
set(CXX_STANDARD_OPTION "")

foreach(ver IN LISTS CXX_STANDARDS)
  set(CMAKE_CXX_STANDARD ${ver})
  set(CMAKE_CXX_STANDARD_REQUIRED ON)

  # Request no GNU extensions by default (matches [noext] in Autotools)
  set(CMAKE_CXX_EXTENSIONS OFF)

  # Check whether CMake provides the compile option variable for this standard
  if(DEFINED CMAKE_CXX${ver}_STANDARD_COMPILE_OPTION AND CMAKE_CXX${ver}_STANDARD_COMPILE_OPTION)
    set(CXX_STANDARD_FOUND TRUE)
    set(CXX_STANDARD_USED ${ver})
    set(CXX_STANDARD_OPTION ${CMAKE_CXX${ver}_STANDARD_COMPILE_OPTION})
    break()
  endif()
endforeach()

# If we found a usable standard, optionally mimic libtool's "double -std=gnu++14 -std=gnu++17"
# behavior for GNU compilers so compile-lines look similar to Autotools output.
# (CMake itself may also add its -std= flag based on CMAKE_CXX_STANDARD/CMAKE_CXX_EXTENSIONS;
# adding these here mirrors the previous libtool lines where libtool injected both flags.)
if(CXX_STANDARD_FOUND AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  if(CXX_STANDARD_USED GREATER 14)
    # prepend lower standard first, then the detected one (libtool-style)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=gnu++14 -std=gnu++${CXX_STANDARD_USED}")
  else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=gnu++${CXX_STANDARD_USED}")
  endif()
endif()

# Finalize ENABLE_CPP behavior like Autotools: AUTO enables only if a baseline (C++14) is found.
if(AUTO_ENABLE_CPP)
  if(CXX_STANDARD_FOUND)
    set(SYSLOG_NG_ENABLE_CPP ON)
    message(STATUS "  C++${CXX_STANDARD_USED} support: enabled (AUTO, found a proper compiler)")
  else()
    set(SYSLOG_NG_ENABLE_CPP OFF)
    message(STATUS "  C++ support: disabled (AUTO, proper compiler not found)")
  endif()
elseif(ENABLE_CPP)
  if(NOT CXX_STANDARD_FOUND)
    message(FATAL_ERROR "ENABLE_CPP was explicitly enabled, but the required C++ compiler dependency could not be found")
  endif()

  set(SYSLOG_NG_ENABLE_CPP ON)
  message(STATUS "  C++${CXX_STANDARD_USED} support: enabled (forced ON)")
endif()
