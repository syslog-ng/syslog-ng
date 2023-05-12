set(GRPC_DEPS_FOUND ${ENABLE_CPP})

module_switch(ENABLE_GRPC "Enable GRPC" GRPC_DEPS_FOUND)

if (NOT ENABLE_GRPC)
  return()
endif()

add_subdirectory(otel)
