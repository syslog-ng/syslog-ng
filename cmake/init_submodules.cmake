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

if(EXISTS "${PROJECT_SOURCE_DIR}/.git")
  option(GIT_SUBMODULE "Checkout submodules during build" ON)

  if(GIT_SUBMODULE)
    find_package(Git QUIET)

    if(NOT GIT_FOUND)
      message(FATAL_ERROR "Git is required but was not found. Please install git and retry.")
    endif()

    message(STATUS "Enumerating and initializing git submodules...")

    # Get all submodule entries from .gitmodules
    execute_process(
      COMMAND "${GIT_EXECUTABLE}" config --file .gitmodules --get-regexp path
      WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
      OUTPUT_VARIABLE _submodule_list
      OUTPUT_STRIP_TRAILING_WHITESPACE
      ERROR_QUIET
    )

    if(_submodule_list STREQUAL "")
      message(STATUS "No git submodules found.")
    else()
      # Split the list into individual lines
      string(REPLACE "\r\n" "\n" _submodule_list "${_submodule_list}")
      string(REPLACE "\r" "\n" _submodule_list "${_submodule_list}")
      string(REPLACE "\n" ";" _submodule_lines "${_submodule_list}")

      foreach(_line IN LISTS _submodule_lines)
        # Each line looks like: submodule.<path>.path <path>
        string(REGEX REPLACE "^submodule\\.([^ ]+)\\.path[ \t]+" "" _path "${_line}")

        if(_path STREQUAL "")
          continue() # Skip malformed lines
        endif()

        if("${_path}" MATCHES "ivykis$")
          if(NOT "${IVYKIS_SOURCE}" STREQUAL "internal" AND NOT("${IVYKIS_SOURCE}" MATCHES "^(auto|AUTO)$"))
            message(STATUS "Skipping ivykis submodule initialization as IVYKIS_SOURCE is set to '${IVYKIS_SOURCE}'")
            continue()
          endif()
        elseif("${_path}" MATCHES "opentelemetry-proto$")
          if(DEFINED ENABLE_GRPC AND NOT ENABLE_GRPC)
            message(STATUS "Skipping grpc submodule ${_path} dependency initialization as ENABLE_GRPC is OFF")
            continue()
          endif()
        elseif("${_path}" MATCHES "jwt-cpp$")
          if(DEFINED ENABLE_CPP AND NOT ENABLE_CPP)
            message(STATUS "Skipping cpp submodule ${_path} dependency initialization as ENABLE_CPP is OFF")
            continue()
          endif()
        else()
          message(FATAL_ERROR "Unknown submodule, please adjust init_submodules.cmake")
        endif()

        message(STATUS "Initializing submodule: ${_path}")
        execute_process(
          COMMAND "${GIT_EXECUTABLE}" submodule update --init "${_path}"
          WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
          RESULT_VARIABLE _sub_result
        )

        if(NOT _sub_result EQUAL 0)
          message(FATAL_ERROR "Failed to initialize submodule '${_path}' (exit code: ${_sub_result})")
        endif()
      endforeach()
    endif()
  endif()
endif()
