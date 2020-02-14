set (SECURE_LOGGING_SOURCES
    secure-logging.c
)

find_package(OpenSSL)

if (OPENSSL_FOUND)
    add_library(secure-logging SHARED ${CRYPTOFUNCS_SOURCES})
    target_include_directories (secure-logging PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_include_directories (secure-logging PRIVATE SYSTEM ${OPENSSL_INCLUDE_DIR})
    target_link_libraries(secure-logging PRIVATE syslog-ng ${OPENSSL_LIBRARIES})

    install(TARGETS secure-logging LIBRARY DESTINATION lib/syslog-ng/ COMPONENT secure-logging)

    add_test_subdirectory(tests)
endif()
