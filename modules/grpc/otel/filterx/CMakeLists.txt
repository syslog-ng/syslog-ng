if(NOT ENABLE_CPP)
  return()
endif()

# C++ code

set(OTEL_LOGRECORD_CPP_SOURCES
  otel-filterx.h
  otel-logrecord.cpp
  otel-logrecord.hpp
  protobuf-field.cpp
  protobuf-field.hpp
)

add_module(
  TARGET otel_filterx_logrecord_cpp
  SOURCES ${OTEL_LOGRECORD_CPP_SOURCES}
  DEPENDS ${MODULE_GRPC_LIBS} grpc-protos
  INCLUDES ${OTEL_PROTO_BUILDDIR} ${PROJECT_SOURCE_DIR}/modules/grpc
  LIBRARY_TYPE STATIC
)
