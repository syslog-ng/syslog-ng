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

set(CMAKE_ADD_C_FLAGS "")
set(CMAKE_ADD_CXX_FLAGS "")

option(ENABLE_MEMTRACE "Enable memtrace" 0)
set(SYSLOG_NG_ENABLE_MEMTRACE ${ENABLE_MEMTRACE})

option(ENABLE_GPROF "Enable gcc profiling" OFF)
set(SYSLOG_NG_ENABLE_GPROF ${ENABLE_GPROF})

if(ENABLE_GPROF)
  set(CMAKE_ADD_C_FLAGS "${CMAKE_ADD_C_FLAGS} -pg")
  set(CMAKE_ADD_CXX_FLAGS "${CMAKE_ADD_CXX_FLAGS} -pg")
endif()

option(ENABLE_GCOV "Enable code coverage analysis (like gcov) support" OFF)
set(SYSLOG_NG_ENABLE_GCOV ${ENABLE_GCOV})

if(ENABLE_GCOV)
  set(CMAKE_ADD_C_FLAGS "${CMAKE_ADD_C_FLAGS} -fprofile-arcs -ftest-coverage")
  set(CMAKE_ADD_CXX_FLAGS "${CMAKE_ADD_CXX_FLAGS} -fprofile-arcs -ftest-coverage")
endif()

# Sanitizer configuration
set(SANITIZER "OFF" CACHE STRING "Enable clang sanitizer")
set_property(CACHE SANITIZER PROPERTY STRINGS OFF address thread undefined)

if(NOT SANITIZER STREQUAL "OFF")
  SET(SANITIZE_MODE "${SANITIZER}")
  message(STATUS "Sanitizer mode: ${SANITIZE_MODE}")

  set(CMAKE_ADD_C_FLAGS "${CMAKE_ADD_C_FLAGS} -O1 -fsanitize=${SANITIZE_MODE} -fno-optimize-sibling-calls")
  set(CMAKE_ADD_CXX_FLAGS "${CMAKE_ADD_CXX_FLAGS} -O1 -fsanitize=${SANITIZE_MODE} -fno-optimize-sibling-calls")

  if(SANITIZER MATCHES "address")
    set(CMAKE_ADD_C_FLAGS "${CMAKE_ADD_C_FLAGS} -fsanitize-address-use-after-scope")
  endif()

  if(APPLE)
    SET(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -fsanitize=${SANITIZE_MODE}")
  endif()
endif()

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${CMAKE_ADD_C_FLAGS}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${CMAKE_ADD_CXX_FLAGS}")

include(enable_perf)
