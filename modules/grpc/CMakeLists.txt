
set(GRPC_DEPS_FOUND FALSE)

if (NOT DEFINED ENABLE_GRPC OR ENABLE_GRPC)
  set(GRPC_DEPS_FOUND TRUE)

  if (NOT ENABLE_CPP)
    if (ENABLE_GRPC)
      message(FATAL_ERROR "C++ support is mandatory when the GRPC modules are enabled.")
    endif()

    set(GRPC_DEPS_FOUND FALSE)
  endif()

  if (GRPC_DEPS_FOUND)
    find_package(Protobuf 3.6.1 QUIET)

    if (NOT Protobuf_FOUND)
      if (ENABLE_GRPC)
        message(FATAL_ERROR "ProtoBuf libraries not found.")
      endif()

      set(GRPC_DEPS_FOUND FALSE)
    else()
      message(STATUS "Found ProtoBuf: ${PROTOBUF_LIBRARIES}")
    endif()
  endif()

  if (GRPC_DEPS_FOUND)
    find_library(GRPC++_LIBRARIES NAMES grpc++)

    if (NOT GRPC++_LIBRARIES)
      if (ENABLE_GRPC)
        message(FATAL_ERROR "gRPC++ libraries not found.")
      endif()

      set(GRPC_DEPS_FOUND FALSE)
    else()
      message(STATUS "Found gRPC++: ${GRPC++_LIBRARIES}")
    endif()
  endif()

  if (GRPC_DEPS_FOUND)
    find_package(gRPC 1.16.1 QUIET)

    if (NOT gRPC_FOUND)
      if (ENABLE_GRPC)
        message(FATAL_ERROR "gRPC libraries not found.")
      endif()

      set(GRPC_DEPS_FOUND FALSE)
    else()
      message(STATUS "Found gRPC: ${GRPC_LIBRARIES}")
    endif()
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

add_subdirectory(protos)
add_subdirectory(otel)
