
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
    # Workaround of https://github.com/protocolbuffers/protobuf/issues/18307
    # The latest (BSD variants) protobuf builds are forcibly bound to libupd, so
    # find_package(gRPC...) will fail with
    #     Targets not yet defined: protobuf::libupb, protobuf::protoc-gen-upb,
    #     protobuf::protoc-gen-upbdefs, protobuf::protoc-gen-upb_minitable
    # Try to satisfy it temporally.
    #
    find_library (UPB_LIBRARIES NAMES upb)
    if (UPB_LIBRARIES)
      add_library(protobuf::libupb STATIC IMPORTED)
      add_executable(protobuf::protoc-gen-upb IMPORTED)
      add_executable(protobuf::protoc-gen-upbdefs IMPORTED)
      add_executable(protobuf::protoc-gen-upb_minitable IMPORTED)
    endif()

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

module_switch(ENABLE_GRPC "Enable GRPC (requires C++17)" GRPC_DEPS_FOUND)

if (ENABLE_GRPC)
  set (CMAKE_CXX_STANDARD 17 CACHE STRING "C++ standard" FORCE)
  set (CMAKE_CXX_STANDARD_REQUIRED ON CACHE BOOL "C++ standard is a requirement" FORCE)

  include(ProtobufGenerateCpp)

  set(MODULE_GRPC_LIBS
    gRPC::grpc
    gRPC::grpc++
    protobuf::libprotobuf)

  add_subdirectory(common)
  add_subdirectory(protos)
endif()

# These are intentionally not inside the above if (ENABLE_GRPC) block
# Let any other possible module_switch-es take effect and is being always visible
# (all modules are protected via ENABLE_GRPC as well)
add_subdirectory(loki)
add_subdirectory(otel)
add_subdirectory(bigquery)
add_subdirectory(pubsub)
add_subdirectory(clickhouse)
