# - Try to find the libnet library
# Once done this will define
#
#  LIBNET_FOUND - system has the libnet library
#  LIBNET_CONFIG
#  LIBNET_LIBRARIES - The libraries needed to use libnet
#
# Based on FindESMTP.cmake
# Distributed under the BSD license.

if (LIBNET_LIBRARIES)
  # Already in cache, be silent
  set(LIBNET_FIND_QUIETLY TRUE)
endif (LIBNET_LIBRARIES)

FIND_PROGRAM(LIBNET_CONFIG libnet-config)

IF (LIBNET_CONFIG)
  EXEC_PROGRAM(${LIBNET_CONFIG} ARGS --libs OUTPUT_VARIABLE _LIBNET_LIBRARIES)
  EXEC_PROGRAM(${LIBNET_CONFIG} ARGS --defines OUTPUT_VARIABLE _LIBNET_DEFINES)
  string(REGEX REPLACE "[\r\n]" " " _LIBNET_LIBRARIES "${_LIBNET_LIBRARIES}")
  string(REGEX REPLACE "[\r\n]" " " _LIBNET_DEFINES "${_LIBNET_DEFINES}")
  set (LIBNET_LIBRARIES ${_LIBNET_LIBRARIES} CACHE STRING "The libraries needed for LIBNET")
  set (LIBNET_DEFINES ${_LIBNET_DEFINES} CACHE STRING "The #defines needed for LIBNET")
  set (LIBNET_FOUND TRUE CACHE BOOL "LibNet is found")
ELSE(LIBNET_CONFIG)
  set (LIBNET_FOUND FALSE CACHE BOOL "LibNet is found")
ENDIF()

include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(LIBNET DEFAULT_MSG LIBNET_LIBRARIES LIBNET_DEFINES LIBNET_FOUND)

MARK_AS_ADVANCED(LIBNET_LIBRARIES)
