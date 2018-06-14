if ENABLE_SQL

module_LTLIBRARIES			+= modules/afsql/libafsql.la
modules_afsql_libafsql_la_SOURCES	= 	\
	modules/afsql/afsql.c 			\
	modules/afsql/afsql.h			\
	modules/afsql/afsql-grammar.y		\
	modules/afsql/afsql-parser.c		\
	modules/afsql/afsql-parser.h		\
	modules/afsql/afsql-plugin.c

modules_afsql_libafsql_la_CPPFLAGS	=	\
	$(AM_CPPFLAGS)				\
	-I$(top_srcdir)/modules/afsql		\
	-I$(top_builddir)/modules/afsql
modules_afsql_libafsql_la_LIBADD	= 	\
	$(MODULE_DEPS_LIBS) $(LIBDBI_LIBS)	\
	$(OPENSSL_LIBS)
modules_afsql_libafsql_la_LDFLAGS	=	\
	$(MODULE_LDFLAGS)
modules_afsql_libafsql_la_DEPENDENCIES	=	\
	$(MODULE_DEPS_LIBS)

modules/afsql modules/afsql/ mod-afsql mod-sql:	\
	modules/afsql/libafsql.la
else
modules/afsql modules/afsql/ mod-afsql mod-sql:
endif

BUILT_SOURCES				+=	\
	modules/afsql/afsql-grammar.y		\
	modules/afsql/afsql-grammar.c		\
	modules/afsql/afsql-grammar.h
EXTRA_DIST				+= 	\
	modules/afsql/afsql-grammar.ym	\
	modules/afsql/CMakeLists.txt

.PHONY: modules/afsql/ mod-afsql mod-sql
