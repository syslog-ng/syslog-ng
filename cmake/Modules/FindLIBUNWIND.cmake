# ############################################################################
# Copyright (c) 2018 Balabit
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

include (LibFindMacros)

libfind_pkg_detect (LIBUNWIND libunwind FIND_PATH libunwind.h FIND_LIBRARY unwind)

if (NOT APPLE)
  libfind_process (LIBUNWIND)
else ()
  # On macOS, libunwind is part of the system libraries
  if (LIBUNWIND_INCLUDE_DIR)
    set (LIBUNWIND_FOUND ON)
    set (LIBUNWIND_LIBRARY "")
    set (LIBUNWIND_LIBRARIES "")
  endif ()
endif ()
