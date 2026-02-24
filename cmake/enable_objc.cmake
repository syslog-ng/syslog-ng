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

if(APPLE)
  # NOTE: Currently the OSLog module is the only one that requires ObjC support
  #       Maintain this depending on the future modules requiring ObjC
  option(ENABLE_OBJC "Enable ObjC" ${ENABLE_DARWIN_OSL})

  if(ENABLE_OBJC)
    message(STATUS "Checking ObjC support")

    # Turned out that even the latest gcc has no proper support of the required ObjC version
    # Once gcc was able to compile our ObjC modules properly this requirement could be removed again
    if(DEFINED CMAKE_OBJC_COMPILER)
      message(STATUS "The ObjC compiler is ${CMAKE_OBJC_COMPILER}")
      get_filename_component(_objc_compiler_name "${CMAKE_OBJC_COMPILER}" NAME)

      if(NOT(_objc_compiler_name MATCHES "^clang(\\+\\+)?([-]?[0-9]+)?$"))
        message(WARNING "ObjC support is only available with clang, forcing CMAKE_OBJC_COMPILER=clang")
        set(CMAKE_OBJC_COMPILER clang)
      endif()
    else()
      set(CMAKE_OBJC_COMPILER clang)
      message(STATUS "The ObjC compiler is ${CMAKE_OBJC_COMPILER}")
    endif()

    set_property(GLOBAL PROPERTY OBJC_STANDARD 11)
    set_property(GLOBAL PROPERTY OBJC_STANDARD_REQUIRED ON)
  endif()
else()
  if(ENABLE_DARWIN_OSL)
    message(FATAL_ERROR "OSLog module enabled, but it is a macOS only feature")
  endif()
endif()
