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

# origin: https://raw.githubusercontent.com/bro/cmake/master/FindLibGeoIP.cmake
# - Try to find GeoIP headers and libraries
# - Patched to use C compiler instead of CXX
#
# Usage of this module as follows:
#
#     find_package(LibGeoIP)
#
# Variables used by this module, they can change the default behaviour and need
# to be set before calling find_package:
#
#  LibGeoIP_ROOT_DIR         Set this variable to the root installation of
#                            libGeoIP if the module has problems finding the
#                            proper installation path.
#
# Variables defined by this module:
#
#  LIBGEOIP_FOUND                   System has GeoIP libraries and headers
#  LibGeoIP_LIBRARY                 The GeoIP library
#  LibGeoIP_INCLUDE_DIR             The location of GeoIP headers
#  HAVE_GEOIP_COUNTRY_EDITION_V6    Whether the API support IPv6 country edition
#  HAVE_GEOIP_CITY_EDITION_REV0_V6  Whether the API supports IPv6 city edition

find_path(LibGeoIP_ROOT_DIR
    NAMES include/GeoIPCity.h
)

if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    # the static version of the library is preferred on OS X for the
    # purposes of making packages (libGeoIP doesn't ship w/ OS X)
    set(libgeoip_names libGeoIp.a GeoIP)
else ()
    set(libgeoip_names GeoIP)
endif ()

find_library(LibGeoIP_LIBRARY
    NAMES ${libgeoip_names}
    HINTS ${LibGeoIP_ROOT_DIR}/lib
)

find_path(LibGeoIP_INCLUDE_DIR
    NAMES GeoIPCity.h
    HINTS ${LibGeoIP_ROOT_DIR}/include
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(LibGeoIP DEFAULT_MSG
    LibGeoIP_LIBRARY
    LibGeoIP_INCLUDE_DIR
)

if (LIBGEOIP_FOUND)
    include(CheckCSourceCompiles)
    set(CMAKE_REQUIRED_INCLUDES ${LibGeoIP_INCLUDE_DIR})
    check_c_source_compiles("
    #include <GeoIPCity.h>
    int main() { GEOIP_COUNTRY_EDITION_V6; return 0; }
    " HAVE_GEOIP_COUNTRY_EDITION_V6)
    check_c_source_compiles("
    #include <GeoIPCity.h>
    int main() { GEOIP_CITY_EDITION_REV0_V6; return 0; }
    " HAVE_GEOIP_CITY_EDITION_REV0_V6)
    set(CMAKE_REQUIRED_INCLUDES)
endif ()

mark_as_advanced(
    LibGeoIP_ROOT_DIR
    LibGeoIP_LIBRARY
    LibGeoIP_INCLUDE_DIR
)
