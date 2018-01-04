set(add_contextual_data_filter_SOURCES
    add-contextual-data-filter.c
    add-contextual-data-filter.h
    add-contextual-data-filter-parser.c
    add-contextual-data-filter-parser.h
    add-contextual-data-filter-plugin.c
    ${CMAKE_CURRENT_BINARY_DIR}/add-contextual-data-filter-grammar.h
    ${CMAKE_CURRENT_BINARY_DIR}/add-contextual-data-filter-grammar.c
)

generate_y_from_ym(modules/add-contextual-data-filter/add-contextual-data-filter-grammar)

bison_target(add_contextual_data_filterGrammar
    ${CMAKE_CURRENT_BINARY_DIR}/add-contextual-data-filter-grammar.y
    ${CMAKE_CURRENT_BINARY_DIR}/add-contextual-data-filter-grammar.c
    COMPILE_FLAGS ${BISON_FLAGS})

include_directories (${CMAKE_CURRENT_BINARY_DIR})
include_directories (${CMAKE_CURRENT_SOURCE_DIR})
include_directories (../add-contextual-data/)
add_library(add_contextual_data_filter SHARED ${add_contextual_data_filter_SOURCES})
target_link_libraries(add_contextual_data_filter PRIVATE syslog-ng)

install(TARGETS add_contextual_data_filter LIBRARY DESTINATION lib/syslog-ng/ COMPONENT add_contextual_data_filter)

add_subdirectory(tests)
