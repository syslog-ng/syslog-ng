metricsincludedir			= ${pkgincludedir}/metrics

EXTRA_DIST += lib/metrics/CMakeLists.txt

metricsinclude_HEADERS = \
	lib/metrics/metrics.h	\
	lib/metrics/dyn-metrics-store.h	\
	lib/metrics/dyn-metrics-cache.h	\
	lib/metrics/metrics-template.h	\
	lib/metrics/label-template.h

metrics_sources = \
	lib/metrics/metrics.c	\
	lib/metrics/dyn-metrics-store.c	\
	lib/metrics/dyn-metrics-cache.c	\
	lib/metrics/metrics-template.c	\
	lib/metrics/label-template.c

#include lib/metrics/tests/Makefile.am
