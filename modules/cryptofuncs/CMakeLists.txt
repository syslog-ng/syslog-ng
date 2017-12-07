set (CRYPTOFUNCS_SOURCES
    cryptofuncs.c
)

find_package(OpenSSL)

if (OPENSSL_FOUND)
    add_library(cryptofuncs SHARED ${CRYPTOFUNCS_SOURCES})
    target_include_directories (cryptofuncs PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_include_directories (cryptofuncs PRIVATE SYSTEM ${OPENSSL_INCLUDE_DIRS})
    target_link_libraries(cryptofuncs PRIVATE syslog-ng ${OPENSSL_LIBRARIES})

    install(TARGETS cryptofuncs LIBRARY DESTINATION lib/syslog-ng/ COMPONENT cryptofuncs)

    add_subdirectory(tests)
endif()
