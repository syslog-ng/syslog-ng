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

# Origin: https://fossies.org/linux/darktable/cmake/modules/FindGettext.cmake
# - Try to find Gettext
# Once done, this will define
#
#  Gettext_FOUND - system has Gettext
#  Gettext_INCLUDE_DIRS - the Gettext include directories
#  Gettext_LIBRARIES - link these to use Gettext
#
# See documentation on how to write CMake scripts at
# http://www.cmake.org/Wiki/CMake:How_To_Find_Libraries

include(LibFindMacros)

# On Linux there is no pkgconfig script, but with this we force
# Gettext_PKGCONF_INCLUDE_DIRS to "". The situation is the same on
# FreeBSD and OS X: Gettext comes without a pkgconfig file.
if(CMAKE_SYSTEM_NAME MATCHES "^(Linux|FreeBSD|Darwin)$")
  set(Gettext_PKGCONF_INCLUDE_DIRS "")
else(CMAKE_SYSTEM_NAME MATCHES "^(Linux|FreeBSD|Darwin)$")
  libfind_pkg_check_modules(Gettext_PKGCONF Gettext)
endif(CMAKE_SYSTEM_NAME MATCHES "^(Linux|FreeBSD|Darwin)$")

if(WIN32 OR APPLE)
  set(Gettext_LIBRARY_SEARCH_DIRS
    /opt/local/lib
    /sw/local/lib
  )

  find_library(Gettext_LIBRARY
    NAMES intl
    PATHS ${Gettext_LIBRARY_SEARCH_DIRS}
    HINTS ${Gettext_PKGCONF_LIBRARY_DIRS}
  )

  set(Gettext_PROCESS_LIBS Gettext_LIBRARY)
endif()

find_path(Gettext_INCLUDE_DIR
  NAMES libintl.h
  HINTS ${Gettext_PKGCONF_INCLUDE_DIRS}
  PATHS /opt/local/include
)

set(Gettext_PROCESS_INCLUDES Gettext_INCLUDE_DIR)
libfind_process(Gettext)
