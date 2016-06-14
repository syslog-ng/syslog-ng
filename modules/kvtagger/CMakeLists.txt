set(kvtagger_SOURCES
    kvtagger.c
    kvtagger.h
    kvtagger-parser.c
    kvtagger-parser.h
    kvtagger-plugin.c
    ${CMAKE_CURRENT_BINARY_DIR}/kvtagger-grammar.h
    ${CMAKE_CURRENT_BINARY_DIR}/kvtagger-grammar.c
)

generate_y_from_ym(modules/kvtagger/kvtagger-grammar)

bison_target(kvtaggerGrammar
    ${CMAKE_CURRENT_BINARY_DIR}/kvtagger-grammar.y
    ${CMAKE_CURRENT_BINARY_DIR}/kvtagger-grammar.c
    COMPILE_FLAGS ${BISON_FLAGS})

include_directories (${CMAKE_CURRENT_BINARY_DIR})
include_directories (${CMAKE_CURRENT_SOURCE_DIR})
add_library(kvtagger MODULE ${kvtagger_SOURCES})
target_link_libraries(kvtagger PRIVATE syslog-ng)

install(TARGETS kvtagger LIBRARY DESTINATION lib/syslog-ng/ COMPONENT kvtagger)
