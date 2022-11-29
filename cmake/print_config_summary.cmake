# ############################################################################
# Copyright (c) 2022 One Identity
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

# Lists actual module definition results
# Behaves similar to the cmake -L option but filters out the relevant information and adds some highlighting
#
function(print_config_summary)
  get_cmake_property(_variableNames VARIABLES)
  list(SORT _variableNames)

  message(STATUS "-----------------------------")
  message(STATUS "syslog-ng Open Source Edition ${SYSLOG_NG_VERSION} configured")
  message(STATUS "-----------------------------")

  string(ASCII 27 Esc)
  set(Red "${Esc}[31m")
  set(Green "${Esc}[32m")

  foreach(_variableName ${_variableNames})
    string(REGEX MATCH "^ENABLE_" _match "${_variableName}")

    if(_match)
      string(LENGTH "${_variableName}" _variableNameLen)
      math(EXPR _variableNameLen "32 - ${_variableNameLen}")
      string(REPEAT " " ${_variableNameLen} _spaces)
      string(REGEX MATCH "^[Oo][Nn]" _on "${${_variableName}}")

      if(_on)
        message("${_variableName}${_spaces}${Green}On")
      else()
        message("${_variableName}${_spaces}${Red}Off")
      endif()
    endif()
  endforeach()
  
endfunction ()
