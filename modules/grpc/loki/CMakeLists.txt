module_switch(ENABLE_LOKI "Enable Loki")
if (NOT ENABLE_LOKI)
  return()
endif()

set(LOKI_SOURCES
    loki-parser.h
    loki-plugin.c
    loki-parser.c
)

add_module(
  TARGET loki
  GRAMMAR loki-grammar
  SOURCES ${LOKI_SOURCES}
)
