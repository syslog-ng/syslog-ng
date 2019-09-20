#############################################################################
# Copyright (c) 2017 Balabit
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

function (add_unit_test)

  if (NOT BUILD_TESTING)
    return()
  endif()

  cmake_parse_arguments(ADD_UNIT_TEST "CRITERION;LIBTEST" "TARGET" "SOURCES;DEPENDS;INCLUDES" ${ARGN})

  if (NOT ADD_UNIT_TEST_SOURCES)
    set(ADD_UNIT_TEST_SOURCES "${ADD_UNIT_TEST_TARGET}.c")
  endif()

  add_executable(${ADD_UNIT_TEST_TARGET} ${ADD_UNIT_TEST_SOURCES})
  target_compile_definitions(${ADD_UNIT_TEST_TARGET} PRIVATE TOP_SRCDIR="${PROJECT_SOURCE_DIR}")
  target_link_libraries(${ADD_UNIT_TEST_TARGET} ${ADD_UNIT_TEST_DEPENDS} syslog-ng)
  target_include_directories(${ADD_UNIT_TEST_TARGET} PUBLIC ${ADD_UNIT_TEST_INCLUDES})
  if (NOT APPLE)
    set_property(TARGET ${ADD_UNIT_TEST_TARGET} APPEND_STRING PROPERTY LINK_FLAGS " -Wl,--no-as-needed")
  endif()

  if (${ADD_UNIT_TEST_CRITERION})
    target_link_libraries(${ADD_UNIT_TEST_TARGET} ${CRITERION_LIBRARIES})
    target_include_directories(${ADD_UNIT_TEST_TARGET} PUBLIC ${CRITERION_INCLUDE_DIRS})
    set_property(TARGET ${ADD_UNIT_TEST_TARGET} PROPERTY POSITION_INDEPENDENT_CODE FALSE)

    # https://gitlab.kitware.com/cmake/cmake/issues/16561
    include(CheckCCompilerFlag)
    check_c_compiler_flag(-no-pie NO_PIE_AVAILABLE)
    if (NO_PIE_AVAILABLE)
      set_property(TARGET ${ADD_UNIT_TEST_TARGET} APPEND_STRING PROPERTY LINK_FLAGS " -no-pie")
    elseif (APPLE)
      set_property(TARGET ${ADD_UNIT_TEST_TARGET} APPEND_STRING PROPERTY LINK_FLAGS " -Wl,-no_pie")
    endif()

  endif()

  if (${ADD_UNIT_TEST_LIBTEST})
    target_link_libraries(${ADD_UNIT_TEST_TARGET} libtest)
  endif()

  add_test (${ADD_UNIT_TEST_TARGET} ${ADD_UNIT_TEST_TARGET})
  set_tests_properties(${ADD_UNIT_TEST_TARGET} PROPERTIES ENVIRONMENT "ASAN_OPTIONS=detect_odr_violation=0;CRITERION_TEST_PATTERN='!(*/*performance*)'")
  set_tests_properties(${ADD_UNIT_TEST_TARGET} PROPERTIES FAIL_REGULAR_EXPRESSION "ERROR: (LeakSanitizer|AddressSanitizer)")
endfunction ()

macro (add_test_subdirectory SUBDIR)
  if (BUILD_TESTING)
    add_subdirectory(${SUBDIR})
  endif()
endmacro()
