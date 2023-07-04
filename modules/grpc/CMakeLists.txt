set(GRPC_DEPS_FOUND ${ENABLE_CPP})

if (GRPC_DEPS_FOUND)
  find_package(Protobuf 3.6.1)
  if (NOT Protobuf_FOUND)
    set(GRPC_DEPS_FOUND FALSE)
  endif()
endif()

if (GRPC_DEPS_FOUND)
  find_package(gRPC 1.16.1)
  if (NOT gRPC_FOUND)
    set(GRPC_DEPS_FOUND FALSE)
  endif()
endif()

module_switch(ENABLE_GRPC "Enable GRPC" GRPC_DEPS_FOUND)

if (NOT ENABLE_GRPC)
  return()
endif()

include(ProtobufGenerateCpp)

set(MODULE_GRPC_LIBS
  gRPC::grpc
  gRPC::grpc++
  protobuf::libprotobuf)

add_subdirectory(otel)
