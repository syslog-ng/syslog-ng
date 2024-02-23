if(NOT ENABLE_GRPC)
  return()
endif()

set(OTEL_CPP_SOURCES
  ${GRPC_CREDENTIALS_SOURCES}
  ${GRPC_METRICS_SOURCES}
  otel-source.cpp
  otel-source.hpp
  otel-source.h
  otel-source-services.hpp
  otel-servicecall.hpp
  otel-logmsg-handles.cpp
  otel-logmsg-handles.hpp
  otel-logmsg-handles.h
  otel-protobuf-parser.hpp
  otel-protobuf-parser.cpp
  otel-protobuf-parser.h
  otel-protobuf-formatter.hpp
  otel-protobuf-formatter.cpp
  otel-dest.hpp
  otel-dest.cpp
  otel-dest.h
  otel-dest-worker.hpp
  otel-dest-worker.cpp
  syslog-ng-otlp-dest.hpp
  syslog-ng-otlp-dest.cpp
  syslog-ng-otlp-dest.h
  syslog-ng-otlp-dest-worker.hpp
  syslog-ng-otlp-dest-worker.cpp)

set(OTEL_SOURCES
  otel-plugin.c
  otel-parser.c
  otel-parser.h
)

add_module(
  TARGET otel-cpp
  SOURCES ${OTEL_CPP_SOURCES}
  DEPENDS ${MODULE_GRPC_LIBS} grpc-protos
  INCLUDES ${OTEL_PROTO_BUILDDIR} ${PROJECT_SOURCE_DIR}/modules/grpc
  LIBRARY_TYPE STATIC
)

add_module(
  TARGET otel
  GRAMMAR otel-grammar
  DEPENDS otel-cpp
  INCLUDES ${PROJECT_SOURCE_DIR}/modules/grpc
  SOURCES ${OTEL_SOURCES}
)

set_target_properties(otel PROPERTIES INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib;${CMAKE_INSTALL_PREFIX}/lib/syslog-ng")

add_subdirectory(filterx)
target_link_libraries(otel PRIVATE otel_filterx_logrecord_cpp)

add_test_subdirectory(tests)
