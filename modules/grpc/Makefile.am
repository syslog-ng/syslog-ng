include modules/grpc/protos/Makefile.am

include modules/grpc/credentials/Makefile.am

include modules/grpc/otel/Makefile.am
include modules/grpc/loki/Makefile.am

if ENABLE_GRPC
modules/grpc modules/grpc/ mod-grpc: mod-otel mod-loki
else
modules/grpc modules/grpc/ mod-grpc:
endif

EXTRA_DIST += \
  modules/grpc/CMakeLists.txt
