if(ENABLE_LOKI AND NOT ENABLE_GRPC)
  message(FATAL_ERROR "GRPC support is mandatory when the Loki modules are enabled.")
endif()

module_switch(ENABLE_LOKI "Enable Loki" ENABLE_GRPC)

if(NOT ENABLE_LOKI OR NOT ENABLE_GRPC)
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
  INCLUDES ${PROJECT_SOURCE_DIR}/modules/grpc
  SOURCES ${LOKI_SOURCES}
)
