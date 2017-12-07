set(LOGGEN_SOURCE
    loggen.c
    ${CMAKE_SOURCE_DIR}/lib/crypto.c
    ${CMAKE_SOURCE_DIR}/lib/compat/openssl_support.c)

add_executable(loggen ${LOGGEN_SOURCE})

include_directories(loggen ${CORE_INCLUDE_DIRS})
target_link_libraries(loggen syslog-ng)

install(TARGETS loggen RUNTIME DESTINATION bin)
