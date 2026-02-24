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

# This is a hard dependency, so we always try to find it.
#
find_package(GLIB 2.32.0 REQUIRED COMPONENTS gmodule gthread)

set(CMAKE_REQUIRED_INCLUDES ${GLIB_INCLUDE_DIRS})
set(CMAKE_REQUIRED_LIBRARIES ${GLIB_LIBRARIES})
check_symbol_exists(g_list_copy_deep "glib.h" SYSLOG_NG_HAVE_G_LIST_COPY_DEEP)
check_symbol_exists(g_memdup2 "glib.h" SYSLOG_NG_HAVE_MEMDUP2)
check_symbol_exists(g_ptr_array_find_with_equal_func "glib.h" SYSLOG_NG_HAVE_G_PTR_ARRAY_FIND_WITH_EQUAL_FUNC)
check_symbol_exists(g_canonicalize_filename "glib.h" SYSLOG_NG_HAVE_G_CANONICALIZE_FILENAME)

# only set MIN_REQUIRED to 2.68 if we know that we are at least on that version.
# On CentOS7/SLES12 we don't have recent enough glib but with compat
# wrappers we can compile. Bad news is that if we don't set to 2.68 at least,
# we can't compile on new platforms with clang.
#
# Best of both worlds: we set to 2.68 if we have that version, so we
# get clang to compile our code, otherwise we set it to 2.32.
if(PC_GLIB_VERSION VERSION_GREATER_EQUAL "2.68")
  set_target_properties(GLib::GLib PROPERTIES
    INTERFACE_COMPILE_DEFINITIONS "GLIB_VERSION_MIN_REQUIRED=GLIB_VERSION_2_68")
else()
  set_target_properties(GLib::GLib PROPERTIES
    INTERFACE_COMPILE_DEFINITIONS "GLIB_VERSION_MIN_REQUIRED=GLIB_VERSION_2_32")
endif()
