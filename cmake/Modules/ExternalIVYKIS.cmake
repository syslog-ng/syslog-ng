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

set(LIB_NAME "IVYKIS")

if (EXISTS ${PROJECT_SOURCE_DIR}/lib/ivykis/src/include/iv.h.in)

    ExternalProject_Add(
        ${LIB_NAME}
        PREFIX            ${CMAKE_CURRENT_BINARY_DIR}
        INSTALL_DIR       ${CMAKE_CURRENT_BINARY_DIR}
        SOURCE_DIR        ${PROJECT_SOURCE_DIR}/lib/ivykis/
        DOWNLOAD_COMMAND  echo
        BUILD_COMMAND     make
        INSTALL_COMMAND   make install
        CONFIGURE_COMMAND autoreconf -i ${PROJECT_SOURCE_DIR}/lib/ivykis && ${PROJECT_SOURCE_DIR}/lib/ivykis/configure --prefix=${CMAKE_CURRENT_BINARY_DIR}
    )

    set(${LIB_NAME}_INTERNAL TRUE)
    set(${LIB_NAME}_INTERNAL_INCLUDE_DIR "${CMAKE_CURRENT_BINARY_DIR}/include" CACHE STRING "${LIB_NAME} include path")
    set(${LIB_NAME}_INTERNAL_LIBRARY "${CMAKE_CURRENT_BINARY_DIR}/lib/libivykis.so" CACHE STRING "${LIB_NAME} library path")
    install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/lib/ DESTINATION lib USE_SOURCE_PERMISSIONS FILES_MATCHING PATTERN "libivykis.so*")

else()
  set(${LIB_NAME}_INTERNAL FALSE)
endif()

