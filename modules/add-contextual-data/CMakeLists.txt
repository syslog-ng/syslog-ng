set(add_contextual_data_SOURCES
    add-contextual-data.c
    add-contextual-data.h
    add-contextual-data-parser.c
    add-contextual-data-parser.h
    add-contextual-data-plugin.c
    context-info-db.h
    context-info-db.c
    contextual-data-record-scanner.h
    contextual-data-record-scanner.c
    csv-contextual-data-record-scanner.h
    csv-contextual-data-record-scanner.c
    add-contextual-data-selector.h
    add-contextual-data-template-selector.h
    add-contextual-data-template-selector.c
    add-contextual-data-filter-selector.h
    add-contextual-data-filter-selector.c
    ${CMAKE_CURRENT_BINARY_DIR}/add-contextual-data-grammar.h
    ${CMAKE_CURRENT_BINARY_DIR}/add-contextual-data-grammar.c
)

generate_y_from_ym(modules/add-contextual-data/add-contextual-data-grammar)

bison_target(add_contextual_dataGrammar
    ${CMAKE_CURRENT_BINARY_DIR}/add-contextual-data-grammar.y
    ${CMAKE_CURRENT_BINARY_DIR}/add-contextual-data-grammar.c
    COMPILE_FLAGS ${BISON_FLAGS})

include_directories (${CMAKE_CURRENT_BINARY_DIR})
include_directories (${CMAKE_CURRENT_SOURCE_DIR})
add_library(add_contextual_data SHARED ${add_contextual_data_SOURCES})
target_link_libraries(add_contextual_data PRIVATE syslog-ng)

install(TARGETS add_contextual_data LIBRARY DESTINATION lib/syslog-ng/ COMPONENT add_contextual_data)

add_test_subdirectory(tests)
