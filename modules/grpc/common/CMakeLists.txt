add_subdirectory(credentials)
add_subdirectory(metrics)

set(GRPC_COMMON_CPP_SOURCES
  ${GRPC_CREDENTIALS_SOURCES}
  ${GRPC_METRICS_SOURCES}
  grpc-dest.hpp
  grpc-dest.cpp
  grpc-dest.h
  grpc-dest-worker.hpp
  grpc-dest-worker.cpp
  grpc-source.hpp
  grpc-source.cpp
  grpc-source.h
  grpc-source-worker.hpp
  grpc-source-worker.cpp)

add_module(
  TARGET grpc-common-cpp
  SOURCES ${GRPC_COMMON_CPP_SOURCES}
  INCLUDES ${PROJECT_SOURCE_DIR}/modules/grpc/common
  LIBRARY_TYPE STATIC
)
