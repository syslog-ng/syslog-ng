#############################################################################
# Copyright (c) 2017 Balabit
# Copyright (c) 2017 Kokan
# Copyright (c) 2025 One Identity
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

# NOTE: This file is used to build the ivykis INTERNAL source
#       no matter what its name suggests.
#
set(LIB_NAME "IVYKIS")

if(EXISTS ${PROJECT_SOURCE_DIR}/lib/ivykis/src/include/iv.h.in)
  IF(CMAKE_BUILD_TYPE MATCHES Debug OR CMAKE_BUILD_TYPE MATCHES RelWithDebInfo)
    set(INTERNAL_IVYKIS_DEBUG_FLAGS "-g -O0")
  ENDIF()

  set(IVY_PREFIX "${CMAKE_CURRENT_BINARY_DIR}/ivykis-install")
  set(IVY_INST   "${CMAKE_CURRENT_BINARY_DIR}/ivykis-install")
  set(IVY_SRC    "${PROJECT_SOURCE_DIR}/lib/ivykis")
  
  set(CONF_CMD  /bin/sh -vc
      "autoreconf -i ${IVY_SRC} && CFLAGS='-fPIC ${CFLAGS} ${INTERNAL_IVYKIS_DEBUG_FLAGS}' ${IVY_SRC}/configure --prefix=${IVY_PREFIX} --disable-shared --enable-static")
  set(BUILD_CMD make)
  set(INST_CMD  make install)

  if (WIN32)
    # --- Windows-only ---
    set(SH "C:/msys64/usr/bin/bash.exe")

    set(IVY_CONF_SH   "${CMAKE_CURRENT_BINARY_DIR}/ivykis-configure.sh")
    set(IVY_BUILD_SH  "${CMAKE_CURRENT_BINARY_DIR}/ivykis-build.sh")
    set(IVY_INSTALL_SH "${CMAKE_CURRENT_BINARY_DIR}/ivykis-install.sh")

    # A tiny bash script to bootstrap ivykis (fix aclocal path, vendor tcl.m4, run libtoolize/autoreconf) and run ./configure in-tree.
    # Avoids MinGW quoting/separator issues and ensures aux files (ltmain.sh, config.guess, etc.) are where configure expects.
    file(WRITE "${IVY_CONF_SH}" "set -e
      export ACLOCAL_PATH='/usr/share/aclocal:/mingw64/share/aclocal'
      mkdir -p '${IVY_SRC}/m4'
      for f in /usr/share/aclocal/tcl.m4 /mingw64/share/aclocal/tcl.m4; do [ -f \"$f\" ] && cp -f \"$f\" '${IVY_SRC}/m4/'; done
      export ACLOCAL=\"aclocal -I '${IVY_SRC}/m4'\"
      cd '${IVY_SRC}'
      rm -rf autom4te.cache
      libtoolize -cfi || true
      autoreconf -fiv
      ./configure --prefix='${IVY_PREFIX}' CFLAGS='-fPIC ${CFLAGS} ${INTERNAL_IVYKIS_DEBUG_FLAGS} --disable-shared --enable-static'
      ")
    file(WRITE "${IVY_BUILD_SH}"   "set -e\ncd '${IVY_SRC}'\nmake -j1 SUBDIRS=src\n")
    file(WRITE "${IVY_INSTALL_SH}" "set -e\ncd '${IVY_SRC}'\nmake SUBDIRS=src install\n")
      
    set(CONF_CMD  ${SH} "${IVY_CONF_SH}")
    set(BUILD_CMD ${SH} "${IVY_BUILD_SH}")
    set(INST_CMD  ${SH} "${IVY_INSTALL_SH}")
  endif()

  ExternalProject_Add(
    ${LIB_NAME}
    PREFIX      ${IVY_PREFIX}
    INSTALL_DIR ${IVY_INST}
    SOURCE_DIR  ${IVY_SRC}

    DOWNLOAD_COMMAND echo
    CONFIGURE_COMMAND ${CONF_CMD}
    BUILD_COMMAND     ${BUILD_CMD}
    INSTALL_COMMAND   ${INST_CMD}
  )

  set(${LIB_NAME}_INTERNAL TRUE)
  set(${LIB_NAME}_INTERNAL_INCLUDE_DIR "${CMAKE_CURRENT_BINARY_DIR}/ivykis-install/include" CACHE STRING "${LIB_NAME} include path")
  set(${LIB_NAME}_INTERNAL_LIBRARY "${CMAKE_CURRENT_BINARY_DIR}/ivykis-install/lib/libivykis.a" CACHE STRING "${LIB_NAME} library path")
  set(SYSLOG_NG_HAVE_IV_WORK_POOL_SUBMIT_CONTINUATION 1 PARENT_SCOPE)

else()
  set(${LIB_NAME}_INTERNAL FALSE)
endif()
