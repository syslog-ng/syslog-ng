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

option(BUILD_TESTING "Enable unit tests" ON)

if(BUILD_TESTING)
  find_package(criterion)

  if(NOT CRITERION_FOUND)
    message(FATAL_ERROR "BUILD_TESTING is enabled (by default, or explicitely) without criterion detected! You can turn off testing via the -DBUILD_TESTING=OFF cmake option.")
  endif()
endif()

include(add_tests)

if(BUILD_TESTING)
  set(CTEST_ENVIRONMENT
    "G_SLICE=always-malloc,debug-blocks"
    "G_DEBUG=fatal-warnings,fatal-criticals,gc-friendly")

  # The inclusion of CTest triggers enable_testing()
  # CMake will generate tests only if the enable_testing() command has been invoked.
  # The CTest module invokes the command automatically when the BUILD_TESTING option is ON.
  include(CTest)

  # This flag might be a security issue, do not use in production code, unfortunately we still need it for criterion tests and the current mocking soution
  if(APPLE)
    SET(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,-flat_namespace")
  endif()

  # Determine parallel job count for tests
  if(NOT DEFINED CTEST_PARALLEL_LEVEL)
    include(ProcessorCount)
    ProcessorCount(N)

    if(NOT N EQUAL 0)
      set(CTEST_PARALLEL_LEVEL ${N})
    else()
      set(CTEST_PARALLEL_LEVEL 1)
    endif()
  endif()

  add_custom_target(check COMMAND ${CMAKE_CTEST_COMMAND} -j ${CTEST_PARALLEL_LEVEL} --output-on-failure)

  # This one is useful to see the failed tests in details
  add_custom_target(check_failed COMMAND ${CMAKE_CTEST_COMMAND} -j ${CTEST_PARALLEL_LEVEL} --rerun-failed --output-on-failure)
endif()
