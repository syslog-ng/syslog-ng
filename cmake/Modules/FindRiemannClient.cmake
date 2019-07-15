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

include(LibFindMacros)

libfind_pkg_detect(Riemann riemann-client FIND_PATH riemann/event.h FIND_LIBRARY riemann-client)
set(Riemann_PROCESS_INCLUDES Riemann_INCLUDE_DIR)
set(Riemann_PROCESS_LIBS Riemann_LIBRARY)

# silence warnings
libfind_process(Riemann QUIET)

set(CMAKE_EXTRA_INCLUDE_FILES "event.h")
set(CMAKE_REQUIRED_INCLUDES "${Riemann_INCLUDE_DIR}/riemann/")
set(CMAKE_REQUIRED_LIBRARIES "${Riemann_LIBRARIES}")

check_type_size(RIEMANN_EVENT_FIELD_TIME_MICROS RIEMANN_MICROSECONDS)
