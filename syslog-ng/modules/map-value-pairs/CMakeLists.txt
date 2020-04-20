module_switch(ENABLE_MAP_VALUE_PAIRS "Enable map-value-pairs")

if (NOT ENABLE_MAP_VALUE_PAIRS)
  return()
endif ()

set(MAP_VALUE_PAIRS_SOURCES
  map-value-pairs-grammar.c
  map-value-pairs-parser.c
  map-value-pairs-plugin.c
  map-value-pairs.c)

add_module(
  TARGET map_value_pairs
  GRAMMAR map-value-pairs-grammar
  SOURCES ${MAP_VALUE_PAIRS_SOURCES}
)

