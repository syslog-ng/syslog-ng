set (CRYPTOFUNCS_SOURCES
    cryptofuncs.c
)


add_library(cryptofuncs SHARED ${CRYPTOFUNCS_SOURCES})
target_include_directories (cryptofuncs PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${OPENSSL_INCLUDE_DIR})
target_link_libraries(cryptofuncs
    PRIVATE
    syslog-ng
    OpenSSL::SSL
    OpenSSL::Crypto
)

install(TARGETS cryptofuncs LIBRARY DESTINATION lib/syslog-ng/ COMPONENT cryptofuncs)

add_test_subdirectory(tests)
