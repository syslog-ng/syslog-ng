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

include(CheckCCompilerFlag)

check_c_compiler_flag("-Wno-initializer-overrides" CMAKE_HAVE_W_NO_INITIALIZER_OVERRIDES)
check_c_compiler_flag("-Wold-style-declaration" CMAKE_HAVE_W_OLD_STYLE_DECLARATION)

set(IMPORTANT_WARNINGS
  -Wshadow)

set(ACCEPTABLE_WARNINGS
  -Wno-stack-protector
  -Wno-unused-parameter
  -Wno-variadic-macros)

option(ENABLE_EXTRA_WARNINGS "Enable extra warnings" ON)
option(ENABLE_WERROR "Enable -Werror" OFF)

set(EXTRA_WARNINGS)

if(ENABLE_EXTRA_WARNINGS)
  set(C_EXTRA_WARNINGS)

  if(CMAKE_HAVE_W_NO_INITIALIZER_OVERRIDES)
    list(APPEND C_EXTRA_WARNINGS
      -Wno-initializer-overrides)
  endif()

  if(CMAKE_HAVE_W_OLD_STYLE_DECLARATION)
    list(APPEND C_EXTRA_WARNINGS
      -Wold-style-declaration)
  endif()

  list(APPEND C_EXTRA_WARNINGS
    -Wimplicit-function-declaration
    -Wnested-externs
    -Wstrict-prototypes
    -Wswitch-default
    -Wimplicit-int
    -Wall
    -Wuninitialized
    -Wunused-but-set-parameter
    -Wdeprecated
    -Wdeprecated-declarations
    -Woverflow
    -Wdouble-promotion
    -Wfloat-equal
    -Wpointer-arith
    -Wpointer-sign
    -Wmissing-format-attribute
    -Wold-style-definition
    -Wundef
    -Wignored-qualifiers
    -Woverride-init
    -Wfloat-conversion
    -Wbad-function-cast)

  set(CXX_EXTRA_WARNINGS
    -Wswitch-default
    -Wall
    -Wuninitialized
    -Wdeprecated
    -Wdeprecated-declarations
    -Woverflow
    -Wdouble-promotion
    -Wfloat-equal
    -Wpointer-arith
    -Wmissing-format-attribute
    -Wundef
    -Wignored-qualifiers
    -Wfloat-conversion
    -Wunused-but-set-parameter)

  foreach(flag IN LISTS C_EXTRA_WARNINGS)
    list(APPEND EXTRA_WARNINGS
      $<$<COMPILE_LANGUAGE:C>:${flag}>)
  endforeach()

  foreach(flag IN LISTS CXX_EXTRA_WARNINGS)
    list(APPEND EXTRA_WARNINGS
      $<$<COMPILE_LANGUAGE:CXX>:${flag}>)
  endforeach()
endif()

if(ENABLE_WERROR)
  set(EXTRA_WARNINGS ${EXTRA_WARNINGS} -Werror)
endif()

add_compile_options(${IMPORTANT_WARNINGS} ${ACCEPTABLE_WARNINGS} ${EXTRA_WARNINGS})

option(ENABLE_FORCE_GNU99 "Enforce C99 with gnu extensions" OFF)

set(CMAKE_C_STANDARD 99)

if(ENABLE_FORCE_GNU99)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=gnu99")
endif()
