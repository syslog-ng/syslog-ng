set(LOGGEN_SOURCE
    loggen.c
    # workaround, crypto and copmat should be a separate static library
    ${CMAKE_SOURCE_DIR}/lib/crypto.c
    ${CMAKE_SOURCE_DIR}/lib/compat/openssl_support.c)

add_executable(loggen ${LOGGEN_SOURCE})
target_link_libraries(loggen syslog-ng)

install(TARGETS loggen RUNTIME DESTINATION bin)
