option(ENABLE_MAP_VALUE_PAIRS "Enable map-value-pairs" ON)

if (NOT ENABLE_MAP_VALUE_PAIRS)
  return()
endif ()

set(MAP_VALUE_PAIRS_SOURCES
  map-value-pairs-grammar.c
  map-value-pairs-parser.c
  map-value-pairs-plugin.c
  map-value-pairs.c)

generate_y_from_ym(modules/map-value-pairs/map-value-pairs-grammar)
bison_target(MapValuePairsExprGrammar
    ${CMAKE_CURRENT_BINARY_DIR}/map-value-pairs-grammar.y
    ${CMAKE_CURRENT_BINARY_DIR}/map-value-pairs-grammar.c
    COMPILE_FLAGS ${BISON_FLAGS})

add_library(map_value_pairs SHARED ${MAP_VALUE_PAIRS_SOURCES})
target_include_directories (map_value_pairs PRIVATE ${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(map_value_pairs PRIVATE syslog-ng)

install(TARGETS map_value_pairs LIBRARY DESTINATION lib/syslog-ng/ COMPONENT map_value_pairs)
