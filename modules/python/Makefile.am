if ENABLE_PYTHON

module_LTLIBRARIES				+= modules/python/libmod-python.la

modules_python_libmod_python_la_CFLAGS		= \
	$(PYTHON_CFLAGS)			  \
	$(SYSLOG_NG_CFLAGS)			  \
	$(EVENTLOG_CFLAGS)			  \
	$(IVYKIS_CFLAGS)			  \
	-I$(top_srcdir)/modules/python		  \
	-I$(top_builddir)/modules/python

modules_python_libmod_python_la_LIBADD		= \
	$(PYTHON_LIBS)				  \
	$(EVENTLOG_LIBS)			  \
	$(SYSLOG_NG_LIBS)			  \
	$(IVYKIS_LIBS)

modules_python_libmod_python_la_SOURCES		= \
	modules/python/python-plugin.c		  \
	modules/python/python-grammar.y		  \
	modules/python/python-dest.c		  \
	modules/python/python-dest.h		  \
	modules/python/python-parser.c		  \
	modules/python/python-parser.h


modules_python_libmod_python_la_LDFLAGS		 = \
	-avoid-version -module -no-undefined	   \
	$(PYTHON_LDADD)

BUILT_SOURCES					+= \
	modules/python/python-grammar.y		   \
	modules/python/python-grammar.c		   \
	modules/python/python-grammar.h

modules/python mod-python: modules/python/libmod-python.la
else
modules/python mod-python:
endif

EXTRA_DIST					+= \
	modules/python/python-grammar.ym	   \
	modules/python/python-example.conf	   \
	modules/python/python-example.py

.PHONY: modules/python mod-python
