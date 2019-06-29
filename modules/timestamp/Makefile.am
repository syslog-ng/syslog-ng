module_LTLIBRARIES += modules/timestamp/libtimestamp.la

modules_timestamp_libtimestamp_la_CPPFLAGS	 = \
	$(AM_CPPFLAGS)				   \
	-I$(top_srcdir)/modules/timestamp		   \
	-I$(top_builddir)/modules/timestamp

modules_timestamp_libtimestamp_la_SOURCES	 = \
	modules/timestamp/timestamp-grammar.y	   \
	modules/timestamp/timestamp-plugin.c	   \
	modules/timestamp/timestamp-parser.c	   \
	modules/timestamp/timestamp-parser.h	   \
	modules/timestamp/rewrite-fix-timezone.c   \
	modules/timestamp/rewrite-fix-timezone.h   \
	modules/timestamp/date-parser.c		   \
	modules/timestamp/date-parser.h

modules_timestamp_libtimestamp_la_LIBADD = \
	$(MODULE_DEPS_LIBS)

modules_timestamp_libtimestamp_la_LDFLAGS	 = \
	$(MODULE_LDFLAGS)
modules_timestamp_libtimestamp_la_DEPENDENCIES	=  \
	$(MODULE_DEPS_LIBS)

BUILT_SOURCES				+= \
	modules/timestamp/timestamp-grammar.y	   \
	modules/timestamp/timestamp-grammar.c	   \
	modules/timestamp/timestamp-grammar.h

include modules/timestamp/tests/Makefile.am

modules/timestamp mod-timestamp: modules/timestamp/libtimestamp.la

EXTRA_DIST += \
	modules/timestamp/timestamp-grammar.ym	\
	modules/timestamp/CMakeLists.txt	\
	modules/timestamp/README.md

.PHONY: modules/timestamp mod-timestamp
