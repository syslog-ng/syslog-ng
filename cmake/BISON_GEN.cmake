#############################################################################
# Copyright (c) 2020 One Identity
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

macro(BISON_GEN)
  cmake_parse_arguments(BISON_GEN "" "NAME;INPUT;OUTPUT" "COMPILE_FLAGS" ${ARGN})
  find_program(SED_EXECUTABLE "sed")

  bison_target(${BISON_GEN_NAME} ${BISON_GEN_INPUT} ${BISON_GEN_OUTPUT}.c_tmp COMPILE_FLAGS ${BISON_GEN_COMPILE_FLAGS})
  add_custom_command(OUTPUT ${BISON_GEN_OUTPUT}.c ${BISON_GEN_OUTPUT}.h
                     COMMAND ${SED_EXECUTABLE} -e '1i\#define SYSLOG_NG_BISON_MAJOR ${BISON_MAJOR}\\n\#define SYSLOG_NG_BISON_MINOR ${BISON_MINOR}\\n' ${BISON_GEN_OUTPUT}.c_tmp > ${BISON_GEN_OUTPUT}.c
                     COMMAND ${CMAKE_COMMAND} -E copy ${BISON_GEN_OUTPUT}.h_tmp ${BISON_GEN_OUTPUT}.h
                     DEPENDS ${BISON_GEN_OUTPUT}.c_tmp ${BISON_GEN_OUTPUT}.h_tmp
                    )
endmacro(BISON_GEN)
