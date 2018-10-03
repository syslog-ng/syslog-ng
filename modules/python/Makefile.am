if ENABLE_PYTHON

module_LTLIBRARIES				+= modules/python/libmod-python.la

modules_python_libmod_python_la_CFLAGS		= \
    $(AM_CFLAGS) \
	$(PYTHON_CFLAGS)			  \
	$(SYSLOG_NG_CFLAGS)			  \
	$(EVENTLOG_CFLAGS)			  \
	$(IVYKIS_CFLAGS)
modules_python_libmod_python_la_CPPFLAGS		= \
    $(AM_CPPFLAGS) \
	-I$(top_srcdir)/modules/python		  \
	-I$(top_builddir)/modules/python

modules_python_libmod_python_la_LIBADD		= \
	$(MODULE_DEPS_LIBS) \
	$(PYTHON_LIBS)				  \
	$(EVENTLOG_LIBS)			  \
	$(SYSLOG_NG_LIBS)			  \
	$(IVYKIS_LIBS)

modules_python_libmod_python_la_SOURCES		= \
	modules/python/compat/compat-python.h     \
	modules/python/compat/compat-python.c     \
	modules/python/python-module.h		  \
	modules/python/python-config.h		  \
	modules/python/python-config.c		  \
	modules/python/python-helpers.h		  \
	modules/python/python-helpers.c		  \
	modules/python/python-main.h		  \
	modules/python/python-main.c		  \
	modules/python/python-plugin.c		  \
	modules/python/python-grammar.y		  \
	modules/python/python-value-pairs.c	  \
	modules/python/python-value-pairs.h	  \
	modules/python/python-dest.c		  \
	modules/python/python-dest.h		  \
	modules/python/python-tf.c		  \
	modules/python/python-tf.h		  \
	modules/python/python-parser.c		  \
	modules/python/python-parser.h	          \
	modules/python/python-logmsg.h		  \
	modules/python/python-logmsg.c	          \
	modules/python/python-logtemplate.h	  \
	modules/python/python-logtemplate.c       \
	modules/python/python-logtemplate-options.h\
	modules/python/python-logtemplate-options.c\
	modules/python/python-integerpointer.c    \
	modules/python/python-integerpointer.h    \
	modules/python/python-global-code-loader.h\
	modules/python/python-global-code-loader.c\
	modules/python/python-debugger.h	  \
	modules/python/python-debugger.c	  \
	modules/python/python-logparser.h	  \
	modules/python/python-logparser.c	  \
	modules/python/python-source.h	  \
	modules/python/python-source.c

modules_python_libmod_python_la_LDFLAGS		 = \
	$(MODULE_LDFLAGS) \
	$(PYTHON_LDADD)

modules_python_libmod_python_la_DEPENDENCIES=	\
	$(MODULE_DEPS_LIBS)

modules/python mod-python: modules/python/libmod-python.la

include modules/python/tests/Makefile.am
else
modules/python mod-python:
endif

include modules/python/pylib/Makefile.am

BUILT_SOURCES					+= \
	modules/python/python-grammar.y		   \
	modules/python/python-grammar.c		   \
	modules/python/python-grammar.h

EXTRA_DIST					+= \
	modules/python/compat/compat-python-v2.c   \
	modules/python/compat/compat-python-v3.c   \
	modules/python/python-grammar.ym	   \
	modules/python/python-example.conf	   \
	modules/python/sngexample.py	\
	modules/python/CMakeLists.txt

.PHONY: modules/python mod-python
