set (SECURE_LOGGING_SOURCES
  secure-logging.c
  slog.c
  slog.h
)

find_package(OpenSSL)

if (OPENSSL_FOUND)
    add_library(secure-logging SHARED ${SECURE_LOGGING_SOURCES})
    target_include_directories (secure-logging PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
    target_include_directories (secure-logging PRIVATE SYSTEM ${OPENSSL_INCLUDE_DIR})
    target_link_libraries(secure-logging PRIVATE syslog-ng ${OPENSSL_LIBRARIES})

    install(TARGETS secure-logging LIBRARY DESTINATION lib/syslog-ng/ COMPONENT secure-logging)

    add_test_subdirectory(slogimport)
    add_test_subdirectory(slogkey)
    add_test_subdirectory(slogverify)
    add_test_subdirectory(tests)
endif()
