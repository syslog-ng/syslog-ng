set(SECURE_LOGGING_SOURCES
  slog.c
  slog.h
  )

add_library(syslog-ng-secure-logging STATIC ${SECURE_LOGGING_SOURCES})
target_include_directories(syslog-ng-secure-logging INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(syslog-ng-secure-logging PRIVATE SYSTEM ${OPENSSL_INCLUDE_DIR})
target_link_libraries(syslog-ng-secure-logging PUBLIC syslog-ng ${OPENSSL_LIBRARIES})

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

add_subdirectory(slogencrypt)
add_subdirectory(slogkey)
add_subdirectory(slogverify)
add_test_subdirectory(tests)
