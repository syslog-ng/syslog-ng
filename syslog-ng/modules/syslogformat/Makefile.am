module_LTLIBRARIES				+= modules/syslogformat/libsyslogformat.la
modules_syslogformat_libsyslogformat_la_SOURCES	=	\
	modules/syslogformat/syslog-format.c		\
	modules/syslogformat/syslog-format.h		\
	modules/syslogformat/syslog-format-plugin.c	\
	modules/syslogformat/syslog-parser-grammar.y	\
	modules/syslogformat/syslog-parser-parser.c	\
	modules/syslogformat/syslog-parser-parser.h	\
	modules/syslogformat/syslog-parser.c		\
	modules/syslogformat/syslog-parser.h

BUILT_SOURCES					+=	\
	modules/syslogformat/syslog-parser-grammar.c	\
	modules/syslogformat/syslog-parser-grammar.h

EXTRA_DIST += modules/syslogformat/CMakeLists.txt \
	modules/syslogformat/syslog-parser-grammar.ym

modules_syslogformat_libsyslogformat_la_CPPFLAGS=	\
	$(AM_CPPFLAGS)					\
	-I${top_srcdir}/modules/syslogformat		\
	-I${top_builddir}/modules/syslogformat

modules_syslogformat_libsyslogformat_la_LIBADD	=	\
	$(MODULE_DEPS_LIBS)
modules_syslogformat_libsyslogformat_la_LDFLAGS	=	\
	$(MODULE_LDFLAGS)
modules_syslogformat_libsyslogformat_la_DEPENDENCIES =	\
	$(MODULE_DEPS_LIBS)

modules/syslogformat modules/syslogformat/ mod-syslogformat: \
	modules/syslogformat/libsyslogformat.la
.PHONY: modules/syslogformat/ mod-syslogformat
