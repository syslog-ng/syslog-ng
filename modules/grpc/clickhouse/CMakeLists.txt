if(NOT ENABLE_GRPC)
  return()
endif()

set(CLICKHOUSE_CPP_SOURCES
  ${GRPC_METRICS_SOURCES}
  clickhouse-dest.hpp
  clickhouse-dest.cpp
  clickhouse-dest.h
  clickhouse-dest-worker.hpp
  clickhouse-dest-worker.cpp
)

set(CLICKHOUSE_SOURCES
  clickhouse-plugin.c
  clickhouse-parser.c
  clickhouse-parser.h
)

add_module(
  TARGET clickhouse-cpp
  SOURCES ${CLICKHOUSE_CPP_SOURCES}
  DEPENDS ${MODULE_GRPC_LIBS} grpc-protos grpc-common-cpp
  INCLUDES ${CLICKHOUSE_PROTO_BUILDDIR} ${PROJECT_SOURCE_DIR}/modules/grpc
  LIBRARY_TYPE STATIC
)

add_module(
  TARGET clickhouse
  GRAMMAR clickhouse-grammar
  DEPENDS clickhouse-cpp grpc-common-cpp
  INCLUDES ${PROJECT_SOURCE_DIR}/modules/grpc
  SOURCES ${CLICKHOUSE_SOURCES}
)

set_target_properties(clickhouse PROPERTIES INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib;${CMAKE_INSTALL_PREFIX}/lib/syslog-ng")
