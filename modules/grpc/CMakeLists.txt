if (NOT ENABLE_CPP AND ENABLE_GRPC)
  message(FATAL_ERROR "C++ support is mandatory when the GRPC modules are enabled.")
endif()

# Default gRPC handling
if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND NOT (CMAKE_SYSTEM_NAME STREQUAL "Linux"))
  if (NOT DEFINED ENABLE_GRPC)
    set(ENABLE_GRPC_WARNING_STR "auto disabled by using GNU g++ on non-Linux OS, see the configure log above" CACHE INTERNAL "GRPC enable warning")
    message(STATUS "${Blue}GRPC support is disabled by default on non-Linux GNU toolchains. "
      "On systems like macOS, the prebuilt dependencies (Protobuf, gRPC, Abseil, etc.) "
      "are usually built with Clang (libc++), so using GNU g++ (libstdc++) may cause build, "
      "link, or runtime issues due to library and ABI mismatches. "
      "If all dependencies were rebuilt with GNU g++, it might work.${ResetFG}"
    )
    module_switch(ENABLE_GRPC "Enable GRPC modules (requires C++20)" OFF)
  elseif (ENABLE_GRPC)
    set(ENABLE_GRPC_WARNING_STR "WARNING: using GNU g++ on non-Linux may cause ABI/library mismatches, see the configure log above" CACHE INTERNAL "GRPC enable warning")
    message(WARNING
      "Building gRPC modules with GNU toolchains on non-Linux systems is unsupported. "
      "On systems like macOS, the prebuilt dependencies (Protobuf, gRPC, Abseil, etc.) "
      "are usually built with Clang (libc++), so using GNU g++ (libstdc++) may cause build, "
      "link, or runtime issues due to library and ABI mismatches. "
      "If all dependencies were rebuilt with GNU g++, it might work. "
      "Proceed only if you understand the potential issues."
    )
  endif()
endif()

set(GRPC_DEPS_FOUND OFF)

if ((NOT DEFINED ENABLE_GRPC OR ENABLE_GRPC) AND (NOT DEFINED ENABLE_CPP OR ENABLE_CPP))
  set(GRPC_DEPS_FOUND ON)

  # FIXME: You cannot remove this otherwise unnecessary if condition and the above
  #        set(GRPC_DEPS_FOUND ON), it causes a strange configure error
  if (GRPC_DEPS_FOUND)
    # Pin a specific protoc/Protobuf install via -DProtobuf_ROOT=<path> (like autotools: --with-protoc).
    find_package(Protobuf 3.6.1 QUIET)

    if (NOT Protobuf_FOUND)
      if (ENABLE_GRPC)
        message(FATAL_ERROR "ProtoBuf libraries not found.")
      endif()

      set(GRPC_DEPS_FOUND OFF)
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

      set(GRPC_DEPS_FOUND OFF)
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

      set(GRPC_DEPS_FOUND OFF)
    else()
      message(STATUS "Found gRPC: ${GRPC_LIBRARIES}")
    endif()
  endif()
endif()

# gRPC requires at least C++17; gracefully disable in AUTO mode if unavailable.
if (GRPC_DEPS_FOUND AND CXX_STANDARD_USED LESS 17)
  if (DEFINED ENABLE_GRPC AND ENABLE_GRPC)
    message(FATAL_ERROR "gRPC modules require at least C++17, but only C++${CXX_STANDARD_USED} support was found.")
  endif()
  set(GRPC_DEPS_FOUND OFF)
endif()

module_switch(ENABLE_GRPC "Enable GRPC modules (requires C++20, or C++17 if C++20 is not available)" GRPC_DEPS_FOUND)

if (ENABLE_GRPC)
  # CXX_STANDARD_USED comes from enable_cpp.cmake (20, 17, or 14)
  if(CXX_STANDARD_USED GREATER_EQUAL 20)
    set(CMAKE_CXX_STANDARD 20 CACHE STRING "C++ standard" FORCE)
  else()
    set(CMAKE_CXX_STANDARD 17 CACHE STRING "C++ standard" FORCE)
  endif()
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
