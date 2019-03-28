if ENABLE_SNMP_DEST

module_LTLIBRARIES				+= modules/snmp-dest/libsnmpdest.la
modules_snmp_dest_libsnmpdest_la_SOURCES	=	\
	modules/snmp-dest/snmpdest-grammar.y \
	modules/snmp-dest/snmpdest.c \
	modules/snmp-dest/snmpdest-plugin.c \
	modules/snmp-dest/snmpdest.h \
	modules/snmp-dest/snmpdest-parser.c \
	modules/snmp-dest/snmpdest-parser.h

modules_snmp_dest_libsnmpdest_la_CPPFLAGS	=	\
	$(AM_CPPFLAGS)					\
	$(NETSNMP_CFLAGS)	\
	-I$(top_srcdir)/modules/snmp-dest		\
	-I$(top_builddir)/modules/snmp-dest

modules_snmp_dest_libsnmpdest_la_LIBADD	=	\
	$(MODULE_DEPS_LIBS)

modules_snmp_dest_libsnmpdest_la_LDFLAGS	=	\
	$(MODULE_LDFLAGS) \
	$(NETSNMP_LIBS)

modules_snmp_dest_libsnmpdest_la_DEPENDENCIES	=	\
	$(MODULE_DEPS_LIBS)

BUILT_SOURCES					+=	\
	modules/snmp-dest/snmpdest-grammar.y \
	modules/snmp-dest/snmpdest-grammar.c \
	modules/snmp-dest/snmpdest-grammar.h

endif

EXTRA_DIST					+=	\
	modules/snmp-dest/snmpdest-grammar.ym	\
	modules/snmp-dest/CMakeLists.txt
