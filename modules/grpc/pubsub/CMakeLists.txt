if(NOT ENABLE_GRPC)
  return()
endif()

set(PUBSUB_CPP_SOURCES
  ${GRPC_METRICS_SOURCES}
  pubsub-dest.hpp
  pubsub-dest.cpp
  pubsub-dest.h
  pubsub-dest-worker.hpp
  pubsub-dest-worker.cpp
)

set(PUBSUB_SOURCES
  pubsub-plugin.c
  pubsub-parser.c
  pubsub-parser.h
)

add_module(
  TARGET pubsub-cpp
  SOURCES ${PUBSUB_CPP_SOURCES}
  DEPENDS ${MODULE_GRPC_LIBS} grpc-protos grpc-common-cpp
  INCLUDES ${PUBSUB_PROTO_BUILDDIR} ${PROJECT_SOURCE_DIR}/modules/grpc
  LIBRARY_TYPE STATIC
)

add_module(
  TARGET pubsub
  GRAMMAR pubsub-grammar
  DEPENDS pubsub-cpp grpc-common-cpp
  INCLUDES ${PROJECT_SOURCE_DIR}/modules/grpc
  SOURCES ${PUBSUB_SOURCES}
)

set_target_properties(pubsub PROPERTIES INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib;${CMAKE_INSTALL_PREFIX}/lib/syslog-ng")
