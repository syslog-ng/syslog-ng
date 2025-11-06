#############################################################################
# Copyright (c) 2016 Balabit
# Copyright (c) 2025 One Identity
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

# This function is syslog-ng specific

function(generate_y_from_ym FileWoExt)
    if (${ARGC} EQUAL 1)
      if (WIN32)
        # clean copies in build/lib
        file(MAKE_DIRECTORY ${PROJECT_BINARY_DIR}/lib)
        configure_file(${PROJECT_SOURCE_DIR}/lib/merge-grammar.py  ${PROJECT_BINARY_DIR}/lib/merge-grammar.py  NEWLINE_STYLE UNIX)
        configure_file(${PROJECT_SOURCE_DIR}/lib/cfg-grammar.y     ${PROJECT_BINARY_DIR}/lib/cfg-grammar.y     NEWLINE_STYLE UNIX)
        configure_file(${PROJECT_SOURCE_DIR}/${FileWoExt}.ym       ${PROJECT_BINARY_DIR}/${FileWoExt}.clean.ym NEWLINE_STYLE UNIX)

        add_custom_command(OUTPUT ${PROJECT_BINARY_DIR}/${FileWoExt}.y
            COMMAND ${PYTHON_LAUNCHER} ${PROJECT_BINARY_DIR}/lib/merge-grammar.py
                    ${PROJECT_BINARY_DIR}/${FileWoExt}.clean.ym > ${PROJECT_BINARY_DIR}/${FileWoExt}.y
            DEPENDS ${PROJECT_BINARY_DIR}/lib/merge-grammar.py
                    ${PROJECT_BINARY_DIR}/lib/cfg-grammar.y
                    ${PROJECT_BINARY_DIR}/${FileWoExt}.clean.ym)
      else()
        add_custom_command (OUTPUT ${PROJECT_BINARY_DIR}/${FileWoExt}.y
            COMMAND ${PYTHON_EXECUTABLE} ${PROJECT_SOURCE_DIR}/lib/merge-grammar.py ${PROJECT_SOURCE_DIR}/${FileWoExt}.ym > ${PROJECT_BINARY_DIR}/${FileWoExt}.y
            DEPENDS ${PROJECT_SOURCE_DIR}/lib/cfg-grammar.y
                    ${PROJECT_SOURCE_DIR}/${FileWoExt}.ym)
      endif()
    elseif(${ARGC} EQUAL 2)
      if (WIN32)
        file(MAKE_DIRECTORY ${PROJECT_BINARY_DIR}/lib)
        configure_file(${PROJECT_SOURCE_DIR}/lib/merge-grammar.py  ${PROJECT_BINARY_DIR}/lib/merge-grammar.py  NEWLINE_STYLE UNIX)
        configure_file(${PROJECT_SOURCE_DIR}/lib/cfg-grammar.y     ${PROJECT_BINARY_DIR}/lib/cfg-grammar.y     NEWLINE_STYLE UNIX)
        configure_file(${PROJECT_SOURCE_DIR}/${FileWoExt}.ym       ${PROJECT_BINARY_DIR}/${FileWoExt}.clean.ym NEWLINE_STYLE UNIX)

        add_custom_command(OUTPUT ${PROJECT_BINARY_DIR}/${ARGV1}.y
            COMMAND ${PYTHON_LAUNCHER} ${PROJECT_BINARY_DIR}/lib/merge-grammar.py
                    ${PROJECT_BINARY_DIR}/${FileWoExt}.clean.ym > ${PROJECT_BINARY_DIR}/${ARGV1}.y
            DEPENDS ${PROJECT_BINARY_DIR}/lib/merge-grammar.py
                    ${PROJECT_BINARY_DIR}/lib/cfg-grammar.y
                    ${PROJECT_BINARY_DIR}/${FileWoExt}.clean.ym)
      else()
        add_custom_command (OUTPUT ${PROJECT_BINARY_DIR}/${ARGV1}.y
            COMMAND ${PYTHON_EXECUTABLE} ${PROJECT_SOURCE_DIR}/lib/merge-grammar.py ${PROJECT_SOURCE_DIR}/${FileWoExt}.ym > ${PROJECT_BINARY_DIR}/${ARGV1}.y
            DEPENDS ${PROJECT_SOURCE_DIR}/lib/cfg-grammar.y
                    ${PROJECT_SOURCE_DIR}/${FileWoExt}.ym)
      endif()
    else()
        message(SEND_ERROR "Wrong usage of generate_y_from_ym() function")
    endif()
endfunction()

# This function is used by add_module
function(module_generate_y_from_ym FileWoExtSrc FileWoExtDst)
    if (${ARGC} EQUAL 2 OR ${ARGC} EQUAL 3)
      set(DEPS ${PROJECT_SOURCE_DIR}/lib/cfg-grammar.y)

      if (${ARGC} EQUAL 3)
        set(DEPS ${DEPS} ${ARGV2})
      endif()

      if (WIN32)
        file(MAKE_DIRECTORY ${PROJECT_BINARY_DIR}/lib)
        configure_file(${PROJECT_SOURCE_DIR}/lib/merge-grammar.py ${PROJECT_BINARY_DIR}/lib/merge-grammar.py NEWLINE_STYLE UNIX)
        configure_file(${PROJECT_SOURCE_DIR}/lib/cfg-grammar.y    ${PROJECT_BINARY_DIR}/lib/cfg-grammar.y    NEWLINE_STYLE UNIX)
        configure_file(${FileWoExtSrc}.ym                         ${FileWoExtDst}.clean.ym                   NEWLINE_STYLE UNIX)

        add_custom_command(OUTPUT ${FileWoExtDst}.y
            COMMAND ${PYTHON_LAUNCHER} ${PROJECT_BINARY_DIR}/lib/merge-grammar.py
                    ${FileWoExtDst}.clean.ym > ${FileWoExtDst}.y
            DEPENDS ${PROJECT_BINARY_DIR}/lib/merge-grammar.py
                    ${PROJECT_BINARY_DIR}/lib/cfg-grammar.y
                    ${FileWoExtDst}.clean.ym
                    ${DEPS})
      else()
        add_custom_command (OUTPUT ${FileWoExtDst}.y
            COMMAND ${PYTHON_EXECUTABLE} ${PROJECT_SOURCE_DIR}/lib/merge-grammar.py ${FileWoExtSrc}.ym > ${FileWoExtDst}.y
            DEPENDS ${DEPS}
            ${FileWoExtSrc}.ym)
      endif()
    else()
        message(SEND_ERROR "Wrong usage of module_generate_y_from_ym() function")
    endif()
endfunction()

