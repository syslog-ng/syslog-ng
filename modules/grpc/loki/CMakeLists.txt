if(NOT ENABLE_GRPC)
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
  DEPENDS ${MODULE_GRPC_LIBS} grpc-protos
  INCLUDES ${PROJECT_SOURCE_DIR}/modules/grpc
  SOURCES ${LOKI_SOURCES}
)
