DIST_SUBDIRS 					+= modules/afmongodb/libmongo-client

if LIBMONGO_INTERNAL
CLEAN_SUBDIRS					+= @LIBMONGO_SUBDIRS@
lmc_EXTRA_DEPS					= modules/afmongodb/libmongo-client/src/libmongo-client.la

modules/afmongodb/libmongo-client/src/libmongo-client.la:
	${MAKE} -C modules/afmongodb/libmongo-client
endif

if ENABLE_MONGODB

module_LTLIBRARIES 				+= modules/afmongodb/libafmongodb.la

modules_afmongodb_libafmongodb_la_CFLAGS 	=	\
	$(LIBMONGO_CFLAGS) 				\
	-I$(top_srcdir)/modules/afmongodb		\
	-I$(top_builddir)/modules/afmongodb
modules_afmongodb_libafmongodb_la_SOURCES	=	\
	modules/afmongodb/afmongodb-grammar.y		\
	modules/afmongodb/afmongodb.c			\
	modules/afmongodb/afmongodb.h			\
	modules/afmongodb/afmongodb-parser.c		\
	modules/afmongodb/afmongodb-parser.h		\
	${DUMMY_C}
modules_afmongodb_libafmongodb_la_LIBADD	=	\
	$(MODULE_DEPS_LIBS) $(CRYPTO_LIBS) $(LIBMONGO_LIBS)
modules_afmongodb_libafmongodb_la_LDFLAGS	=	\
	$(MODULE_LDFLAGS)
modules_afmongodb_libafmongodb_la_DEPENDENCIES	=	\
	$(MODULE_DEPS_LIBS) $(CRYPTO_LIBS) ${lmc_EXTRA_DEPS}

modules/afmongodb modules/afmongodb/ mod-afmongodb mod-mongodb: \
	modules/afmongodb/libafmongodb.la
else
modules/afmongodb modules/afmongodb/ mod-afmongodb mod-mongodb:
endif

modules/afmongodb/dummy.c: ${lmc_EXTRA_DEPS}
	$(AM_V_GEN) touch $@

BUILT_SOURCES					+=	\
	modules/afmongodb/afmongodb-grammar.y		\
	modules/afmongodb/afmongodb-grammar.c		\
	modules/afmongodb/afmongodb-grammar.h		\
	modules/afmongodb/dummy.c
EXTRA_DIST					+=	\
	modules/afmongodb/afmongodb-grammar.ym		\
	modules/afmongodb/libmongo-client/configure.gnu

.PHONY: modules/afmongodb/ mod-afmongodb mod-mongodb
