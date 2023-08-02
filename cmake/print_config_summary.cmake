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

function(_print_summary_line _variableName _maxVarNameLen)
  string(ASCII 27 Esc)
  set(Red "${Esc}[31m")
  set(Green "${Esc}[32m")
  set(ResetFG "${Esc}[39m")
  set(_variableValue "${${_variableName}}")

  string(LENGTH "${_variableName}" _variableNameLen)
  math(EXPR _variableNameLen "${_maxVarNameLen} - ${_variableNameLen}")

  if(_variableNameLen LESS 0)
    set(_variableNameLen 0)
  endif()

  string(REPEAT " " ${_variableNameLen} _spaces)
  string(TOUPPER "${_variableValue}" _upperValue)

  if(_upperValue STREQUAL "ON" OR _upperValue STREQUAL "TRUE")
    message("${_variableName}${_spaces}${Green}On${ResetFG}")
  elseif(_upperValue STREQUAL "OFF" OR _upperValue STREQUAL "FALSE")
    message("${_variableName}${_spaces}${Red}Off${ResetFG}")
  else()
    message("${_variableName}${_spaces}${Green}${_variableValue}${ResetFG}")
  endif()
endfunction()

# Lists actual module definition results
# Behaves similar to the cmake -L option but filters out the relevant information and adds some highlighting
#
function(print_config_summary)
  list(APPEND _importantVariableNames "IVYKIS_INTERNAL" "BUILD_TESTING")
  get_cmake_property(_variableNames VARIABLES)
  list(SORT _variableNames)

  message(NOTICE "-----------------------------------")
  message(NOTICE "syslog-ng Open Source Edition ${SYSLOG_NG_VERSION} configured")
  message(NOTICE "-----------------------------------")

  if(SUMMARY_VERBOSE)
    foreach(_variableName ${_variableNames})
      string(REGEX MATCH ".*(_LIBRARY|_LIBRARIES)$" _match "${_variableName}")
      string(REGEX MATCH "^(CMAKE_|_).*" _cmakeInternal "${_variableName}")

      if(_match AND NOT _cmakeInternal)
        _print_summary_line(${_variableName} 40)
      endif()
    endforeach()
  endif()

  message(NOTICE "-----------------------------------")

  foreach(_variableName ${_variableNames})
    foreach(_importantVariableName ${_importantVariableNames})
      string(REGEX MATCH "^${_importantVariableName}$" _match "${_variableName}")

      if(_match)
        _print_summary_line(${_importantVariableName} 32)
      endif()
    endforeach()
  endforeach()

  message(NOTICE "-----------------------------------")

  foreach(_variableName ${_variableNames})
    string(REGEX MATCH "^ENABLE_" _match "${_variableName}")

    if(_match)
      _print_summary_line(${_variableName} 32)
    else()
      if(SUMMARY_FULL)
        message("${_variableName}=${${_variableName}}")
      endif()
    endif()
  endforeach()

  message(NOTICE "-----------------------------------")
endfunction()
