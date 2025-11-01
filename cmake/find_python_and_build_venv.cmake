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

module_switch(ENABLE_PYTHON "Enable Python module" ON)

if(NOT ENABLE_PYTHON)
  return()
endif()

set(PYTHON_VERSION "AUTO" CACHE STRING "Version of the installed development library")

# --- Normalize Python version syntax like in Automake ---
set(_ver "${PYTHON_VERSION}")

if(_ver MATCHES "^[0-9]+$")
  # Single digit version like "3" → python3
  set(PYTHON_VERSION_NAME "python${_ver}")
elseif(_ver MATCHES "^[0-9]+\\.[0-9]+$")
  # Version like "3.8" or "3.10" → python-3.8
  set(PYTHON_VERSION_NAME "python-${_ver}")
else()
  # Otherwise keep as-is (e.g. python3, python3.11, etc.)
  set(PYTHON_VERSION_NAME "${_ver}")
endif()

# --- Fix case sensitivity e.g. for macOS (Python vs python) ---
# Build a module-style name from the normalized version name (python3 / python-3.8 etc.)
string(REGEX REPLACE "^python" "Python" PYTHON_MODULE_NAME "${PYTHON_VERSION_NAME}")
message(STATUS "Searching for Python version: ${PYTHON_MODULE_NAME}")

if(_ver MATCHES "^(auto|AUTO)$")
  # If version is "auto", allow any version
  set(PYTHON_VERSION_NAME "")
  set(PYTHON_MODULE_NAME "Python")
endif()

# --- Detect Python installation (prefer -embed, like autotools) ---
set(PYTHON_FOUND FALSE)

# Try embedded version first (e.g. Python3-embed)
if(NOT PYTHON_FOUND)
  find_package(${PYTHON_MODULE_NAME} COMPONENTS Interpreter Development.Embed QUIET)

  if(${PYTHON_MODULE_NAME}_FOUND)
    message(STATUS "Found embedded Python: ${PYTHON_MODULE_NAME}")
    set(PYTHON_FOUND TRUE)
  endif()
endif()

# Fallback: try regular development version (e.g. Python3)
if(NOT PYTHON_FOUND)
  find_package(${PYTHON_MODULE_NAME} COMPONENTS Interpreter Development QUIET)

  if(${PYTHON_MODULE_NAME}_FOUND)
    message(STATUS "Found regular Python: ${PYTHON_MODULE_NAME}")
    set(PYTHON_FOUND TRUE)
  endif()
endif()

# Final fallback for legacy CMake (PythonInterp/PythonLibs)
if(NOT PYTHON_FOUND)
  cmake_policy(SET CMP0148 OLD)

  if(PYTHON_VERSION_NAME STREQUAL "")
    find_package(PythonInterp EXACT 3)
    find_package(PythonLibs EXACT 3)
  else()
    find_package(PythonInterp EXACT "${PYTHON_VERSION}" REQUIRED)
    find_package(PythonLibs EXACT "${PYTHON_VERSION}" REQUIRED)
  endif()

  if(PYTHONLIBS_FOUND)
    message(STATUS "Found Python ${PYTHON_VERSION_NAME} using legacy PythonInterp/PythonLibs fallback")
    set(PYTHON_FOUND TRUE)
  endif()
endif()

if(PYTHON_FOUND)
  # Backward compatibility with legacy CMake variables
  if(NOT DEFINED PYTHON_INCLUDE_DIRS AND DEFINED ${PYTHON_MODULE_NAME}_INCLUDE_DIRS)
    set(PYTHON_INCLUDE_DIRS "${${PYTHON_MODULE_NAME}_INCLUDE_DIRS}" CACHE PATH "Python include dirs")
  endif()

  if(NOT DEFINED PYTHON_LIBRARIES AND DEFINED ${PYTHON_MODULE_NAME}_LIBRARIES)
    set(PYTHON_LIBRARIES "${${PYTHON_MODULE_NAME}_LIBRARIES}" CACHE FILEPATH "Python libraries")
  endif()

  if(NOT DEFINED PYTHON_EXECUTABLE AND DEFINED ${PYTHON_MODULE_NAME}_EXECUTABLE)
    set(PYTHON_EXECUTABLE "${${PYTHON_MODULE_NAME}_EXECUTABLE}" CACHE FILEPATH "Python executable")
  endif()

  if(NOT DEFINED PYTHONLIBS_VERSION_STRING AND DEFINED ${PYTHON_MODULE_NAME}_VERSION)
    set(PYTHONLIBS_VERSION_STRING "${${PYTHON_MODULE_NAME}_VERSION}" CACHE STRING "Python version string")
  endif()

  message(STATUS "Detected pythonlib version: ${PYTHONLIBS_VERSION_STRING}")
else()
  set(PYTHON_VERSION_NAME "none")
endif()

if(ENABLE_PYTHON AND NOT PYTHON_FOUND)
  message(FATAL_ERROR "Python module was explicitly enabled, but the required python library dependency could not be found")
endif()

set(ENABLE_PYTHON ${PYTHON_FOUND})

include(python_build_venv)

set(ENABLE_PYTHON_INFO_STR "pkg-config package: ${PYTHONLIBS_VERSION_STRING}, packages: ${PYTHON_PACKAGES_INSTALL}" CACHE INTERNAL "Python version and packages install mode info")