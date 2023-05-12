include modules/grpc/otel/Makefile.am

if ENABLE_GRPC
modules/grpc modules/grpc/ mod-grpc: mod-otel
endif

.PHONY: modules/grpc/otel/ mod-otel

EXTRA_DIST += \
  modules/grpc/CMakeLists.txt
