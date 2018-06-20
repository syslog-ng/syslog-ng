#############################################################################
# Copyright (c) 2017 Balabit
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

libfind_pkg_check_modules(CRITERION_PKGCONF criterion)

libfind_pkg_detect(CRITERION criterion FIND_PATH criterion/criterion.h FIND_LIBRARY criterion)
set(CRITERION_PROCESS_INCLUDES CRITERION_INCLUDE_DIR)
set(CRITERION_PROCESS_LIBS CRITERION_LIBRARY)
libfind_process(CRITERION)

if (NOT CRITERION_FOUND)
    MESSAGE(WARNING "Criterion not found: some unit tests will not be compiled")
endif()
