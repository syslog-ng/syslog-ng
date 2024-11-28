if(NOT ENABLE_GRPC)
  return()
endif()

set(OTEL_CPP_SOURCES
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
  DEPENDS ${MODULE_GRPC_LIBS} grpc-protos grpc-common-cpp
  INCLUDES ${OTEL_PROTO_BUILDDIR} ${PROJECT_SOURCE_DIR}/modules/grpc
  LIBRARY_TYPE STATIC
)

add_module(
  TARGET otel
  GRAMMAR otel-grammar
  DEPENDS otel-cpp grpc-common-cpp
  INCLUDES ${PROJECT_SOURCE_DIR}/modules/grpc
  SOURCES ${OTEL_SOURCES}
)

set_target_properties(otel PROPERTIES INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib;${CMAKE_INSTALL_PREFIX}/lib/syslog-ng")

target_compile_options (otel-cpp PRIVATE -Wno-double-promotion -Wno-deprecated -DPROTOBUF_ENABLE_DEBUG_LOGGING_MAY_LEAK_PII=0)

add_subdirectory(filterx)
target_link_libraries(otel PRIVATE otel_filterx_logrecord_cpp)

add_test_subdirectory(tests)
