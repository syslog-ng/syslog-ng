#############################################################################
# Copyright (c) 2016 Balabit
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

# some parts have been copied from https://raw.githubusercontent.com/WebKit/webkit/master/Source/cmake/FindGLIB.cmake
include(LibFindMacros)

libfind_pkg_check_modules(GLIB_GTHREAD_PKGCONF gthread-2.0)
libfind_pkg_check_modules(GLIB_GMODULE_PKGCONF gmodule-2.0)
libfind_pkg_check_modules(GLIB_PKGCONF glib-2.0)

find_path(GLIB_INCLUDE_DIR
  NAMES glib.h
  PATHS ${GLIB_PKGCONF_INCLUDE_DIRS}
  PATH_SUFFIXES glib-2.0
)

find_path(GLIBCONFIG_INCLUDE_DIR
  NAMES glibconfig.h
  HINTS ${GLIB_PKGCONF_INCLUDE_DIRS} ${GLIB_PKGCONF_INCLUDE_DIRS}/include
  PATH_SUFFIXES glib-2.0/include
)

find_library(GLIB_LIBRARY
  NAMES glib-2.0
  PATHS ${GLIB_PKGCONF_LIBRARY_DIRS}
)

find_library(GLIB_GTHREAD_LIBRARIES
  NAMES gthread-2.0
  PATHS ${GLIB_PKGCONF_LIBRARY_DIRS}
)

find_library(GLIB_GMODULE_LIBRARIES
  NAMES gmodule-2.0
  PATHS ${GLIB_PKGCONF_LIBRARY_DIRS}
)

# Version detection
file(READ "${GLIBCONFIG_INCLUDE_DIR}/glibconfig.h" GLIBCONFIG_H_CONTENTS)
string(REGEX MATCH "#define GLIB_MAJOR_VERSION ([0-9]+)" _dummy "${GLIBCONFIG_H_CONTENTS}")
set(GLIB_VERSION_MAJOR "${CMAKE_MATCH_1}")
string(REGEX MATCH "#define GLIB_MINOR_VERSION ([0-9]+)" _dummy "${GLIBCONFIG_H_CONTENTS}")
set(GLIB_VERSION_MINOR "${CMAKE_MATCH_1}")
string(REGEX MATCH "#define GLIB_MICRO_VERSION ([0-9]+)" _dummy "${GLIBCONFIG_H_CONTENTS}")
set(GLIB_VERSION_MICRO "${CMAKE_MATCH_1}")
set(GLIB_VERSION "${GLIB_VERSION_MAJOR}.${GLIB_VERSION_MINOR}.${GLIB_VERSION_MICRO}")

# Set the include dir variables and the libraries and let libfind_process do the rest.
# NOTE: Singular variables for this library, plural for libraries this this lib depends on.
set(GLIB_PROCESS_INCLUDES GLIB_INCLUDE_DIR GLIBCONFIG_INCLUDE_DIR)
set(GLIB_PROCESS_LIBS GLIB_LIBRARY GLIB_GTHREAD_LIBRARY GLIB_GMODULE_LIBRARY)
libfind_process(GLIB)
