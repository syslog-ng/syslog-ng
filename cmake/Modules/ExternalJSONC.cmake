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

set(LIB_NAME "JSONC")

if (EXISTS ${PROJECT_SOURCE_DIR}/lib/jsonc/json.h)

    ExternalProject_Add(
        ${LIB_NAME}
        PREFIX            ${CMAKE_CURRENT_BINARY_DIR}
        INSTALL_DIR       ${CMAKE_CURRENT_BINARY_DIR}
        SOURCE_DIR        ${PROJECT_SOURCE_DIR}/lib/jsonc/
        DOWNLOAD_COMMAND  echo
        BUILD_COMMAND     make
        INSTALL_COMMAND   make install
        CONFIGURE_COMMAND ${PROJECT_SOURCE_DIR}/lib/jsonc/configure  --prefix=${PROJECT_BINARY_DIR}
        STEP_TARGETS update patch configure
    )

   ExternalProject_Add_Step(${LIB_NAME} autogen
     COMMAND ./autogen.sh
     COMMENT "Performing autogen step for '${LIB_NAME}'"
     DEPENDEES update patch
     DEPENDERS configure
     WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/lib/jsonc/
     LOG 1
     )

    set(${LIB_NAME}_INTERNAL TRUE)
    set(${LIB_NAME}_INTERNAL_INCLUDE_DIR "${PROJECT_BINARY_DIR}/include/json-c/")
    set(${LIB_NAME}_INTERNAL_LIBRARY "${PROJECT_BINARY_DIR}/lib/libjson-c.so")
    install(DIRECTORY ${PROJECT_BINARY_DIR}/lib/ DESTINATION lib USE_SOURCE_PERMISSIONS FILES_MATCHING PATTERN "libjson-c.so*")

else()
  set(${LIB_NAME}_INTERNAL FALSE)
endif()
