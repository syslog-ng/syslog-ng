set(KVFORMAT_SOURCES
    format-welf.c
    format-welf.h
    kv-parser.c
    kv-parser.h
    linux-audit-parser.c
    linux-audit-parser.h
    kv-parser-parser.c
    kv-parser-parser.h
    kvformat-plugin.c
    ${CMAKE_CURRENT_BINARY_DIR}/kv-parser-grammar.c
    ${CMAKE_CURRENT_BINARY_DIR}/kv-parser-grammar.h
)

generate_y_from_ym(modules/kvformat/kv-parser-grammar)

bison_target(KVFormatGrammar
    ${CMAKE_CURRENT_BINARY_DIR}/kv-parser-grammar.y
    ${CMAKE_CURRENT_BINARY_DIR}/kv-parser-grammar.c
    COMPILE_FLAGS ${BISON_FLAGS})

include_directories (${CMAKE_CURRENT_BINARY_DIR})
include_directories (${CMAKE_CURRENT_SOURCE_DIR})
add_library(kvformat SHARED ${KVFORMAT_SOURCES})
target_link_libraries(kvformat PRIVATE syslog-ng)

install(TARGETS kvformat LIBRARY DESTINATION lib/syslog-ng/ COMPONENT kvformat)

add_test_subdirectory(tests)
