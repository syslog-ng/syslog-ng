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

set(SYSLOG_NG_USE_CONST_IVYKIS_MOCK 1)
set(IVYKIS_SOURCE "internal" CACHE STRING "Select ivykis source (internal/system/path/AUTO)")
external_or_find_package(IVYKIS REQUIRED)
set(IVYKIS_LIBRARIES ${IVYKIS_LIBRARY} CACHE STRING "IVYKIS libraries path")

if((NOT IVYKIS_INTERNAL) AND (IVYKIS_PKGCONF_VERSION VERSION_LESS "0.39"))
  set(SYSLOG_NG_USE_CONST_IVYKIS_MOCK 0)
elseif(IVYKIS_INTERNAL)
  # internal ivykis is linked statically, io_uring might be available and linked against too, we have to use it as well
  find_package(uring)

  if(URING_FOUND)
    set(IVYKIS_LIBRARIES ${IVYKIS_LIBRARIES} ${URING_LIBRARIES})
  endif()
endif()

include(find_inotify)
