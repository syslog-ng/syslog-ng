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

set(IMPORTANT_WARNINGS
  -Wshadow)

set(ACCEPTABLE_WARNINGS
  -Wno-stack-protector
  -Wno-unused-parameter
  -Wno-variadic-macros)

option(ENABLE_EXTRA_WARNINGS "Enable extra warnings" ON)

if(ENABLE_EXTRA_WARNINGS)
  set(EXTRA_WARNINGS
    $<$<COMPILE_LANGUAGE:C>:-Wimplicit-function-declaration>
    $<$<COMPILE_LANGUAGE:C>:-Wnested-externs>
    $<$<COMPILE_LANGUAGE:C>:-Wstrict-prototypes>
    -Wswitch-default
    $<$<COMPILE_LANGUAGE:C>:-Wimplicit-int>
    -Wall
    -Wuninitialized
    -Wdeprecated
    -Wdeprecated-declarations
    -Woverflow
    -Wdouble-promotion
    -Wfloat-equal
    -Wpointer-arith
    $<$<COMPILE_LANGUAGE:C>:-Wpointer-sign>
    -Wmissing-format-attribute
    $<$<COMPILE_LANGUAGE:C>:-Wold-style-definition>
    -Wundef
    -Wignored-qualifiers
    -Wfloat-conversion
    $<$<COMPILE_LANGUAGE:C>:-Wbad-function-cast>)

  if("${CMAKE_C_COMPILER_ID}" MATCHES "Clang")
    set(EXTRA_WARNINGS
      ${EXTRA_WARNINGS}
    )
  else()
    set(EXTRA_WARNINGS
      $<$<COMPILE_LANGUAGE:C>:-Wold-style-declaration>
      -Wunused-but-set-parameter
      $<$<COMPILE_LANGUAGE:C>:-Woverride-init>
      ${EXTRA_WARNINGS}
    )
  endif()
endif()

add_compile_options(${IMPORTANT_WARNINGS} ${ACCEPTABLE_WARNINGS} ${EXTRA_WARNINGS})

option(ENABLE_FORCE_GNU99 "Enforce C99 with gnu extensions" OFF)

set(CMAKE_C_STANDARD 99)

if(ENABLE_FORCE_GNU99)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=gnu99")
endif()
