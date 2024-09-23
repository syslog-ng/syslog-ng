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

string(ASCII 27 Esc)
set(Red "${Esc}[31m")
set(Green "${Esc}[32m")
set(Yellow "${Esc}[33m")
set(ResetFG "${Esc}[39m")

set(_maxHeaderLen 39)

list(APPEND _envInfo "CMAKE_HOST_SYSTEM" "CMAKE_HOST_SYSTEM_NAME" "CMAKE_HOST_SYSTEM_PROCESSOR" "CMAKE_HOST_SYSTEM_VERSION")
list(APPEND _subModules "IVYKIS_SOURCE")
list(APPEND _compilersInfo "CMAKE_C_COMPILER" "CMAKE_CXX_COMPILER" "CMAKE_OBJC_COMPILER")
list(APPEND _compilationsOptions "CMAKE_BUILD_TYPE" "BUILD_TESTING" "ENABLE_EXTRA_WARNINGS")
if(APPLE)
  list(APPEND _compilationsOptions "FORCE_CLASSIC_LINKING")
endif()

option(SUMMARY_LEVEL "Detail level of the cmake options summary information (0,1 or 2)" 0)

function(_space_tabbed_string _variableName _maxVarNameLen _outputVariable)
  set(_spaces "")

  if(_maxVarNameLen GREATER 0)
    string(LENGTH "${_variableName}" _variableNameLen)
    math(EXPR _variableNameLen "${_maxVarNameLen} - ${_variableNameLen}")

    if(_variableNameLen LESS 0)
      set(_variableNameLen 0)
    endif()

    string(REPEAT " " ${_variableNameLen} _spaces)
  endif()

  set(${_outputVariable} "${_variableName}${_spaces}" PARENT_SCOPE)
  # return(PROPAGATE ${_outputVariable})
endfunction()

function(_print_summary_line _variableName _variableValue _maxVarNameLen)
  set(_spaceTabbedVariableName "${_variableName}")

  _space_tabbed_string("${_variableName}" ${_maxVarNameLen} _spaceTabbedVariableName)

  string(TOUPPER "${_variableValue}" _upperValue)

  if(_upperValue STREQUAL "ON" OR _upperValue STREQUAL "TRUE")
    message("${_spaceTabbedVariableName}${Green}On${ResetFG}")
  elseif(_upperValue STREQUAL "OFF" OR _upperValue STREQUAL "FALSE")
    message("${_spaceTabbedVariableName}${Red}Off${ResetFG}")
  else()
    message("${_spaceTabbedVariableName}${White}${_variableValue}${ResetFG}")
  endif()
endfunction()

function(_print_separator _header)
  string(LENGTH "${_header}" _headerLen)
  if(_headerLen LESS_EQUAL 0)
    string(REPEAT "-" ${_maxHeaderLen} _header)
  else()
    math(EXPR _headerSeparatorLen "(${_maxHeaderLen} - ${_headerLen} - 2) / 2")
    math(EXPR _total_len "((${_headerSeparatorLen} * 2) + ${_headerLen} + 2)")
    set(_padding "")
    if(_total_len LESS ${_maxHeaderLen})
      set(_padding "-")
    endif()
    string(REPEAT "-" ${_headerSeparatorLen} _headerSeparator)
    set(_header "${_headerSeparator} ${_header} ${_headerSeparator}${_padding}")
  endif()
  message(NOTICE "${_header}")
endfunction()

function(_print_libraries _variableNames)
  foreach(_variableName ${_variableNames})
    string(REGEX MATCH ".*(_FOUND)$" _match_found "${_variableName}")
    string(REGEX MATCH ".*(_LIBRARY|_LIBRARIES|_LIBRARY_OPTS)$" _match_lib "${_variableName}")
    string(REGEX MATCH ".*(INCLUDE_DIR|INCLUDEDIR|INCLUDE_DIRS|INCLUDE_OPTS)$" _match_incl "${_variableName}")
    string(REGEX MATCH "^(CMAKE_|_).*" _cmakeInternal "${_variableName}")

    if((_match_lib OR _match_incl OR _match_found) AND NOT _cmakeInternal)
      _print_summary_line("${_variableName}=" "${${_variableName}}" 0)
    endif()
  endforeach()
endfunction()

function(_print_compilers_info _variableNames _importantVariableNames)
  foreach(_importantVariableName ${_importantVariableNames})
    foreach(_variableName ${_variableNames})
      string(REGEX MATCH "^${_importantVariableName}$" _match "${_variableName}")

      if(_match)
        string(REGEX MATCH ".*(_COMPILER)$" _isCompiler "${_variableName}")

        if(_isCompiler)
          set(_compilerVersion "")
          get_filename_component (_executable_name "${${_variableName}}" NAME_WE)
          execute_process (
            COMMAND ${${_variableName}} --version
            COMMAND grep -i ${_executable_name}
            OUTPUT_VARIABLE _compilerVersion
            OUTPUT_STRIP_TRAILING_WHITESPACE
          )
          execute_process (
            COMMAND which ${${_variableName}}
            OUTPUT_VARIABLE _compilerPath
            OUTPUT_STRIP_TRAILING_WHITESPACE
          )
          _space_tabbed_string("${_variableName}" 32 _spaceTabbedVariableName)
          _print_summary_line("${_spaceTabbedVariableName}${_executable_name}" "${Green}[${Yellow}${_compilerVersion}${Green}]${ResetFG} - ${_compilerPath}" 40)
          unset(_compilerVersion)
        else()
          _print_summary_line("${_importantVariableName}" "${${_importantVariableName}}" 32)
        endif()
      endif()
    endforeach()
  endforeach()
endfunction()

function(_print_module_options _variableNames)
  foreach(_variableName ${_variableNames})
    string(REGEX MATCH "^ENABLE_" _match "${_variableName}")
    list(FIND _compilationsOptions "${_variableName}" _index)

    if(_match AND _index LESS 0)
      _print_summary_line("${_variableName}" "${${_variableName}}" 32)
    endif()
  endforeach()
endfunction()

function(_print_options _variableNames _options)
  foreach (_option ${_options})
    list(FIND _variableNames "${_option}" _index)

    if(_index GREATER_EQUAL 0)
      _print_summary_line("${_option}" "${${_option}}" 32)
    endif()
  endforeach ()
endfunction ()

function(_print_full _variableNames)
  foreach(_variableName ${_variableNames})
    message("${_variableName}=${${_variableName}}")
  endforeach()
endfunction()

# Lists actual module definition results
# Behaves similar to the cmake -L option but filters out the relevant information and adds some highlighting
#
function(print_config_summary)
  get_cmake_property(_variableNames VARIABLES)
  list(SORT _variableNames)
  list(REMOVE_DUPLICATES _variableNames)

  if(SUMMARY_FULL OR SUMMARY_LEVEL GREATER_EQUAL 2)
    _print_separator("")
    _print_full("${_variableNames}")
  else()
    if(SUMMARY_VERBOSE OR SUMMARY_LEVEL EQUAL 1)
      _print_separator("")
      _print_libraries("${_variableNames}")
    endif()
  endif()

  _print_separator("")
  message (NOTICE "syslog-ng Open Source Edition ${SYSLOG_NG_VERSION} configured")

  _print_separator("Environment")
  _print_options ("${_variableNames}" "${_envInfo}")

  _print_separator("Compilers")
  _print_compilers_info("${_variableNames}" "${_compilersInfo}")

  _print_separator("Compilation")
  _print_options("${_variableNames}" "${_compilationsOptions}")

  _print_separator("Sub-modules")
  _print_options("${_variableNames}" "${_subModules}")

  _print_separator("Modules")
  _print_module_options("${_variableNames}")

  _print_separator("")
endfunction()
