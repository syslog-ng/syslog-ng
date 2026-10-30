module_switch(ENABLE_SLOG "Enable secure logging (slog) module and CLI tools from Airbus" OFF)

if(NOT ENABLE_SLOG)
  return()
endif()

# --- utils_slog Static Library ---
# Contains the shared utility functions
set(UTILS_SLOG_SOURCES
  utils_slog.h
  utils_slog.c
)

add_library(utils_slog STATIC ${UTILS_SLOG_SOURCES})

# This ensures glib.h can be found during compilation
target_include_directories(utils_slog PUBLIC ${GLIB_INCLUDE_DIRS})

# This ensures we pick up the local headers (utils_slog.h)
target_include_directories(utils_slog PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# Ensure GLib and other dependencies are found
target_link_libraries(utils_slog PUBLIC ${GLIB_LIBRARIES})
target_include_directories(utils_slog PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# --- syslog-ng-secure-logging Static Library ---
# Contains the core slog logic
set(SECURE_LOGGING_SOURCES
  slog.h
  slog.c
)

add_library(syslog-ng-secure-logging STATIC ${SECURE_LOGGING_SOURCES})
target_include_directories(syslog-ng-secure-logging INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(syslog-ng-secure-logging PRIVATE SYSTEM ${OPENSSL_INCLUDE_DIR})

# Link to utils_slog here
target_link_libraries(syslog-ng-secure-logging
  PUBLIC
    syslog-ng
    utils_slog
    ${OPENSSL_LIBRARIES}
)

# --- secure-logging Module ---
# The loadable plugin for syslog-ng

set(SECURE_LOGGING_MODULE_SOURCES
  secure-logging.c
  )

add_module(
  TARGET secure-logging
  DEPENDS syslog-ng-secure-logging
          OpenSSL::SSL
          OpenSSL::Crypto
  SOURCES ${SECURE_LOGGING_MODULE_SOURCES}
)

# --- Subdirectories ---
add_subdirectory(slogencrypt)
add_subdirectory(slogkey)
add_subdirectory(slogverify)
add_test_subdirectory(tests)
