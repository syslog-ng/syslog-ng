module_LTLIBRARIES				+= \
	modules/snmp-format/libsnmp-format.la

modules_snmp_format_libsnmp_format_la_SOURCES	=	\
	modules/snmp-format/snmp-format.c		\
	modules/snmp-format/snmp-format.h		\
	modules/snmp-format/snmp-format-plugin.c

modules_snmp_format_libsnmp_format_la_CPPFLAGS	=	\
	$(AM_CPPFLAGS)					\
	-I$(top_srcdir)/modules/snmp-format		\
	-I$(top_builddir)/modules/snmp-format
modules_snmp_format_libsnmp_format_la_LIBADD	= 	\
	$(MODULE_DEPS_LIBS)

modules_snmp_format_libsnmp_format_la_LDFLAGS	=	\
	$(MODULE_LDFLAGS)
modules_snmp_format_libsnmp_format_la_DEPENDENCIES =	\
	$(MODULE_DEPS_LIBS)

modules/snmp-format modules/snmp-format/ mod-snmp: modules/snmp-format/libsnmp-format.la

.PHONY: modules/snmp-format/ mod-snmp-format

include modules/snmp-format/tests/Makefile.am
