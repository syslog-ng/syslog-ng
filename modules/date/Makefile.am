module_LTLIBRARIES += modules/date/libdate.la

modules_date_libdate_la_CPPFLAGS	 = \
	$(AM_CPPFLAGS)				   \
	-I$(top_srcdir)/modules/date		   \
	-I$(top_builddir)/modules/date

modules_date_libdate_la_SOURCES	 = \
	modules/date/date-grammar.y		   \
	modules/date/date-plugin.c		   \
	modules/date/date-parser.c		   \
	modules/date/date-parser.h		   \
	modules/date/date-parser-parser.c	   \
	modules/date/date-parser-parser.h	   \
	modules/date/strptime-tz.c	           \
	modules/date/strptime-tz.h

modules_date_libdate_la_LIBADD = \
	$(MODULE_DEPS_LIBS)

modules_date_libdate_la_LDFLAGS	 = \
	$(MODULE_LDFLAGS)
modules_date_libdate_la_DEPENDENCIES	=  \
	$(MODULE_DEPS_LIBS)

BUILT_SOURCES				+= \
	modules/date/date-grammar.y	   \
	modules/date/date-grammar.c	   \
	modules/date/date-grammar.h

include modules/date/tests/Makefile.am

modules/date mod-date: modules/date/libdate.la

EXTRA_DIST += \
	modules/date/date-grammar.ym	\
	modules/date/CMakeLists.txt	\
	modules/date/README.md

.PHONY: modules/date mod-date
