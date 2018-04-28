set(PSEUDOFILE_SOURCES
    pseudofile.c
    pseudofile.h
    pseudofile-parser.c
    pseudofile-parser.h
    pseudofile-plugin.c
    ${CMAKE_CURRENT_BINARY_DIR}/pseudofile-grammar.c
    ${CMAKE_CURRENT_BINARY_DIR}/pseudofile-grammar.h
)

generate_y_from_ym(modules/pseudofile/pseudofile-grammar)

bison_target(PseudoFileGrammar
    ${CMAKE_CURRENT_BINARY_DIR}/pseudofile-grammar.y
    ${CMAKE_CURRENT_BINARY_DIR}/pseudofile-grammar.c
    COMPILE_FLAGS ${BISON_FLAGS})

add_library(pseudofile MODULE ${PSEUDOFILE_SOURCES})
target_include_directories(pseudofile
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
  PRIVATE ${CMAKE_CURRENT_BINARY_DIR}
)
target_link_libraries(pseudofile PRIVATE syslog-ng)

install(TARGETS pseudofile LIBRARY DESTINATION lib/syslog-ng/)
