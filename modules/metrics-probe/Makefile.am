module_LTLIBRARIES				+= modules/metrics-probe/libmetrics-probe.la
modules_metrics_probe_libmetrics_probe_la_SOURCES	=	\
	modules/metrics-probe/metrics-probe.c			\
	modules/metrics-probe/metrics-probe.h			\
	modules/metrics-probe/metrics-probe-grammar.y		\
	modules/metrics-probe/metrics-probe-parser.c		\
	modules/metrics-probe/metrics-probe-parser.h		\
	modules/metrics-probe/metrics-probe-plugin.c

modules_metrics_probe_libmetrics_probe_la_CPPFLAGS	=	\
	$(AM_CPPFLAGS)					\
	-I$(top_srcdir)/modules/metrics-probe		\
	-I$(top_builddir)/modules/metrics-probe
modules_metrics_probe_libmetrics_probe_la_LIBADD	=	\
	$(MODULE_DEPS_LIBS)
modules_metrics_probe_libmetrics_probe_la_LDFLAGS	=	\
	$(MODULE_LDFLAGS)
EXTRA_modules_metrics_probe_libmetrics_probe_la_DEPENDENCIES	=	\
	$(MODULE_DEPS_LIBS)

BUILT_SOURCES					+=	\
	modules/metrics-probe/metrics-probe-grammar.y		\
	modules/metrics-probe/metrics-probe-grammar.c		\
	modules/metrics-probe/metrics-probe-grammar.h
EXTRA_DIST					+=	\
	modules/metrics-probe/metrics-probe-grammar.ym	\
	modules/metrics-probe/CMakeLists.txt

modules/metrics-probe modules/metrics-probe/ mod-metrics-probe: modules/metrics-probe/libmetrics-probe.la
.PHONY: modules/metrics-probe/ mod-metrics-probe

include modules/metrics-probe/tests/Makefile.am
