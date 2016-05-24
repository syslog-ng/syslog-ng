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

include(FindPackageHandleStandardArgs)

find_program(GRADLE_EXECUTABLE NAMES gradle PATHS $ENV{GRADLE_HOME}/bin NO_CMAKE_FIND_ROOT_PATH)

if (GRADLE_EXECUTABLE)
    execute_process(COMMAND gradle --version
                    COMMAND grep Gradle
                    COMMAND head -1
                    COMMAND sed "s/.*\\ \\(.*\\)/\\1/"
                    OUTPUT_VARIABLE GRADLE_VERSION
                    OUTPUT_STRIP_TRAILING_WHITESPACE
                    )
endif()

find_package_handle_standard_args(Gradle FOUND_VAR GRADLE_FOUND  REQUIRED_VARS GRADLE_EXECUTABLE VERSION_VAR GRADLE_VERSION)
mark_as_advanced(GRADLE_EXECUTABLE)
