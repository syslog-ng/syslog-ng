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

set(_maxHeaderLen 43)
set(_maxSummaryLineLen 36)
set(_orig_maxSummaryLineLen "${_maxSummaryLineLen}")

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

function(_get_matching_options _result_list _variableNames _options)
  if(ARGN)
    set(_exceptNames ${ARGN})
  endif()

  foreach(_variableName ${_variableNames})
    foreach(_option ${_options})
      string(REGEX MATCH "${_option}" _match "${_variableName}")

      if(DEFINED _exceptNames)
        set(_except_index -1)
        foreach(_exceptName ${_exceptNames})
          string(REGEX MATCH "${_exceptName}" _except_match "${_variableName}")
          if(_except_match)
            set(_except_index 1)
            break()
          endif()
        endforeach()
      else()
        set(_except_index -1)
      endif()

      if(_match AND _except_index LESS 0)
        list(APPEND new_list "${_variableName}")
      endif()
    endforeach()
  endforeach()
  set(${_result_list} "${new_list}" PARENT_SCOPE)
endfunction()

function(_get_feature_list _result_list _variableNames _exceptNames)
  _get_matching_options(_filteredNames "${_variableNames}" "SYSLOG_NG_ENABLE_" "${_exceptName}")

  foreach(_variableName ${_filteredNames})
    string(REPLACE "SYSLOG_NG_" "" _featureName "${_variableName}")
    list(APPEND _final_list "${_featureName}")
  endforeach()
  set(${_result_list} "${_final_list}" PARENT_SCOPE)
endfunction()

function(colored_on_off value outVar)
  string(TOUPPER "${value}" _upperValue)
  if(_upperValue STREQUAL "ON" OR _upperValue STREQUAL "TRUE" OR _upperValue STREQUAL "YES")
    set(_result "${Green}On")
  elseif(_upperValue STREQUAL "OFF" OR _upperValue STREQUAL "FALSE" OR _upperValue STREQUAL "NO")
    set(_result "${Red}Off")
  else()
    set(_result "")
  endif()
  set(${outVar} "${_result}" PARENT_SCOPE)
endfunction()

function(_print_summary_line _variableName _variableValue _maxVarNameLen)
  # Check if variable name starts with ENABLE_ and corresponding _VERSION_STR exists
  set(_extraString "")
  string(REGEX MATCH "^ENABLE_.*" _var_match "${_variableName}")
  if(_var_match)
    set(_versionVar "${_variableName}_INFO_STR")
    if(DEFINED ${_versionVar})
      set(_extraString " (${${_versionVar}})")
    endif()
    set(_versionVar "${_variableName}_WARNING_STR")
    if(DEFINED ${_versionVar})
      set(_extraString " (${Yellow}${${_versionVar}}${Blue})")
    endif()
  endif()

  set(_spaceTabbedVariableName "${_variableName}")
  _space_tabbed_string("${_variableName}" ${_maxVarNameLen} _spaceTabbedVariableName)

  string(TOUPPER "${_variableValue}" _upperValue)

  colored_on_off("${_upperValue}" _on_off_str)
  if(_on_off_str)
    message("${_spaceTabbedVariableName}${_on_off_str}${Blue}${_extraString}${ResetFG}")
  elseif(_upperValue STREQUAL "AUTO")
    set(_autoValueVar "SYSLOG_NG_${_variableName}")

    if(DEFINED ${_autoValueVar})
      colored_on_off("${${_autoValueVar}}" _on_off_str)
      set(_extraString " (${_on_off_str}${Blue})")
    endif()
    message("${_spaceTabbedVariableName}${Blue}Auto${_extraString}${ResetFG}")
  else()
    message("${_spaceTabbedVariableName}${Blue}${_variableValue}${ResetFG}")
  endif()
endfunction()

function(_print_separator _header)
  if(ARGN)
    set(_total_Len ${ARGN})
  else()
    set(_total_Len ${_maxHeaderLen})
  endif()

  string(LENGTH "${_header}" _headerLen)
  if(_headerLen LESS_EQUAL 0)
    string(REPEAT "-" ${_total_Len} _header)
  else()
    math(EXPR _headerSeparatorLen "(${_total_Len} - ${_headerLen} - 2) / 2")
    math(EXPR _total_len "((${_headerSeparatorLen} * 2) + ${_headerLen} + 2)")
    set(_padding "")
    if(_total_len LESS ${_total_Len})
      set(_padding "-")
    endif()
    string(REPEAT "-" ${_headerSeparatorLen} _headerSeparator)
    set(_header "${_headerSeparator} ${_header} ${_headerSeparator}${_padding}")
  endif()
  message(NOTICE "${_header}")
endfunction()

function(_print_compilers_info _variableNames _importantVariableNames)
  foreach(_importantVariableName ${_importantVariableNames})
    foreach(_variableName ${_variableNames})
      string(REGEX MATCH "^${_importantVariableName}$" _match "${_variableName}")

      if(_match)
        string(REGEX MATCH ".*(_COMPILER)$" _isCompiler "${_variableName}")

        if(_isCompiler)
          set(_compilerVersion "")
          get_filename_component(_executable_name "${${_variableName}}" NAME_WE)
          execute_process(
            COMMAND which ${${_variableName}}
            OUTPUT_VARIABLE _compilerPath
            OUTPUT_STRIP_TRAILING_WHITESPACE
          )
          execute_process(
            COMMAND "${_compilerPath}" --version
            # version info does not always contain the exact executable name (clang++ -> clang)
            # using now the hardcoded first line content instead :S
            # COMMAND grep -i ${_executable_name}
            COMMAND head -n 1
            OUTPUT_VARIABLE _compilerVersion
            OUTPUT_STRIP_TRAILING_WHITESPACE
          )
          _space_tabbed_string("${_variableName}" ${_maxSummaryLineLen} _spaceTabbedVariableName)
          _space_tabbed_string("${_executable_name}" 10 _spaceTabbedExecutable_name)
          _print_summary_line("${_spaceTabbedVariableName}" "${_spaceTabbedExecutable_name}${Green}[${Yellow}${_compilerVersion}${Green}]${ResetFG} - ${_compilerPath}" ${_maxSummaryLineLen})
          unset(_compilerVersion)
        else()
          _print_summary_line("${_importantVariableName}" "${${_importantVariableName}}" ${_maxSummaryLineLen})
        endif()
      endif()
    endforeach()
  endforeach()
endfunction()

function(_print_options _variableNames _options)
  if(_options)
    _get_matching_options(_result_list "${_variableNames}" "${_options}" ${ARGN})
  else()
    set(_result_list "${_variableNames}")
  endif()
  foreach(_variableName ${_result_list})
    _print_summary_line("${_variableName}" "${${_variableName}}" ${_maxSummaryLineLen})
  endforeach()
endfunction()

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
      set(_maxSummaryLineLen 46)
      _print_separator("" ${_maxSummaryLineLen})
      list(APPEND _libraryOptions ".*(_FOUND)$" ".*(_LIBRARY|_LIBRARIES|_LIBRARY_OPTS)$" ".*(INCLUDE_DIR|INCLUDEDIR|INCLUDE_DIRS|INCLUDE_OPTS)$" ".*(_CFLAGS|_CFLAGS_OTHER)$")
      list(APPEND _libraryExcludeOptions "^(CMAKE_|_).*")
      _print_options("${_variableNames}" "${_libraryOptions}" "${_libraryExcludeOptions}")

      set(_maxSummaryLineLen 56)
      _print_separator("" ${_maxSummaryLineLen})
      list(APPEND _syslogngOptions "^SYSLOG_NG_HAVE_.*")
      _print_options("${_variableNames}" "${_syslogngOptions}" "")

      set(_maxSummaryLineLen "${_orig_maxSummaryLineLen}")
    endif()
  endif()

  _print_separator("")
  message(NOTICE "syslog-ng Open Source Edition ${SYSLOG_NG_VERSION} configured")

  _print_separator("Environment")
  list(APPEND _envInfo "^CMAKE_HOST_SYSTEM")
  _print_options("${_variableNames}" "${_envInfo}")

  _print_separator("Compilers")
  list(APPEND _compilersInfo "CMAKE_C_COMPILER" "CMAKE_CXX_COMPILER" "CMAKE_OBJC_COMPILER")
  _print_compilers_info("${_variableNames}" "${_compilersInfo}")

  _print_separator("Compilation")
  list(APPEND _compilationOptions "CMAKE_BUILD_TYPE" "CXX_STANDARD_USED" "^ENABLE_CPP" "ENABLE_EXTRA_WARNINGS" "ENABLE_FORCE_GNU99" "ENV_LD_LIBRARY_PATH")

  if(APPLE)
    list(APPEND _compilationOptions "FORCE_CLASSIC_LINKING")
  endif()
  _print_options("${_variableNames}" "${_compilationOptions}")

  _print_separator("Testing")
  list(APPEND _testingOptions "BUILD_TESTING")
  _print_options("${_variableNames}" "${_testingOptions}")

  _print_separator ("Man pages")
  list (APPEND _manPages "^ENABLE_MANPAGES" "WITH_MANPAGES")
  _print_options ("${_variableNames}" "${_manPages}")

  _get_matching_options(_evaluatedFeaturesOptions "${_variableNames}" "^SYSLOG_NG_ENABLE_")

  _print_separator("Features")
  list(APPEND _featuresExcludeOptions "ENABLE_DEBUG" "ENABLE_CPP" "ENABLE_AFSOCKET_MEMINFO_METRICS" "ENABLE_MANPAGES")
  _get_feature_list(_featuresOptions "${_evaluatedFeaturesOptions}" "${_featuresExcludeOptions}")
  list(APPEND _featuresOptions "SANITIZER")
  _print_options("${_featuresOptions}" "")

  if(SUMMARY_FULL OR SUMMARY_VERBOSE OR SUMMARY_LEVEL GREATER_EQUAL 1)
    set(_maxSummaryLineLen 42)
    _print_separator("Evaluated features" ${_maxSummaryLineLen})
    list(APPEND _evaluatedFeaturesOptions "SYSLOG_NG_SYSTEMD_JOURNAL_MODE")
    _print_options("${_evaluatedFeaturesOptions}" "")
    set(_maxSummaryLineLen "${_orig_maxSummaryLineLen}")
  endif()

  _print_separator("Sub-modules")
  list(APPEND _subModules "IVYKIS_SOURCE")
  _print_options("${_variableNames}" "${_subModules}")

  _print_separator("Modules")
  list(APPEND _modulesOptions "^ENABLE_")
  list(APPEND _modulesExcludeOptions "${_compilationOptions}" "${_featuresOptions}" "^ENABLE_.*_INFO_STR$" "^ENABLE_.*_WARNING_STR$" "^ENABLE_MANPAGES" "^ENABLE_SPOOF_SOURCE")
  _print_options("${_variableNames}" "${_modulesOptions}" "${_modulesExcludeOptions}")

  _print_separator("")
endfunction()

print_config_summary()