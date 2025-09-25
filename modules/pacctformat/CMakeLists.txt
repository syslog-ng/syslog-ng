if(CMAKE_SYSTEM_NAME STREQUAL Linux)
  set(IS_LINUX "ON")
else()
  set(IS_LINUX "OFF")
endif()

module_switch(ENABLE_PACCT "Enable support for reading Process Accounting files (EXPERIMENTAL, Linux only)." IS_LINUX)

if(ENABLE_PACCT AND NOT IS_LINUX)
  message(FATAL_ERROR "Process Accounting was explicitly enabled, but the OS is not Linux")
endif()

if(NOT ENABLE_PACCT)
  return()
endif()

set(PACCT_FORMAT_SOURCES
  pacct-format.c
  pacct-format.h
  pacct-format-plugin.c
)

add_module(
  TARGET pacctformat
  SOURCES ${PACCT_FORMAT_SOURCES}
)
