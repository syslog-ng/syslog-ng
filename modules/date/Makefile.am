module_LTLIBRARIES += modules/date/libdate.la

modules_date_libdate_la_CPPFLAGS	 = \
	$(AM_CPPFLAGS)				   \
	-I$(top_srcdir)/modules/date		   \
	-I$(top_builddir)/modules/date

modules_date_libdate_la_SOURCES	 = \
	modules/date/date-parser-grammar.y	   \
	modules/date/date-parser.c		   \
	modules/date/date-parser.h		   \
	modules/date/date-parser-parser.c	   \
	modules/date/date-parser-parser.h	   \
	modules/date/date-parser-plugin.c

modules_date_libdate_la_LDFLAGS	 = \
	$(MODULE_LDFLAGS)
modules_date_libdate_la_DEPENDENCIES	=  \
	$(MODULE_DEPS_LIBS)

BUILT_SOURCES				+= \
	modules/date/date-parser-grammar.y	   \
	modules/date/date-parser-grammar.c	   \
	modules/date/date-parser-grammar.h

include modules/date/tests/Makefile.am

modules/date mod-date: modules/date/libdate.la

EXTRA_DIST += \
	modules/date/date-parser-grammar.ym

.PHONY: modules/date mod-date
