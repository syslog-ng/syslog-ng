DIST_SUBDIRS 					+= @LIBMONGO_SUBDIRS@

if LIBMONGO_INTERNAL
CLEAN_SUBDIRS					+= @LIBMONGO_SUBDIRS@
lmc_EXTRA_DEPS					= @LIBMONGO_SUBDIRS@/libmongoc-1.0.la

$(lmc_EXTRA_DEPS):
	${MAKE} -C modules/afmongodb/mongo-c-driver
endif

if ENABLE_MONGODB

module_LTLIBRARIES 				+= modules/afmongodb/libafmongodb.la

modules_afmongodb_libafmongodb_la_CFLAGS 	=	\
    $(AM_CFLAGS) \
	$(LIBMONGO_CFLAGS) 				\
	-I$(top_srcdir)/modules/afmongodb		\
	-I$(top_builddir)/modules/afmongodb
modules_afmongodb_libafmongodb_la_SOURCES	=	\
	modules/afmongodb/afmongodb-grammar.y		\
	modules/afmongodb/afmongodb.c			\
	modules/afmongodb/afmongodb.h			\
	modules/afmongodb/afmongodb-private.h			\
	modules/afmongodb/afmongodb-parser.c		\
	modules/afmongodb/afmongodb-parser.h		\
	${DUMMY_C}

if ENABLE_LEGACY_MONGODB_OPTIONS
modules_afmongodb_libafmongodb_la_SOURCES	+=	\
	modules/afmongodb/afmongodb-legacy-uri.c			\
	modules/afmongodb/afmongodb-legacy-uri.h			\
	modules/afmongodb/afmongodb-legacy-grammar.c			\
	modules/afmongodb/afmongodb-legacy-grammar.h			\
	modules/afmongodb/afmongodb-legacy-private.h			\
	modules/afmongodb/host-list.c				\
	modules/afmongodb/host-list.h
endif

modules_afmongodb_libafmongodb_la_LIBADD	=	\
	$(MODULE_DEPS_LIBS) $(LIBMONGO_LIBS)
modules_afmongodb_libafmongodb_la_LDFLAGS	=	\
	$(MODULE_LDFLAGS)
modules_afmongodb_libafmongodb_la_DEPENDENCIES	=	\
	$(MODULE_DEPS_LIBS) $(lmc_EXTRA_DEPS)

modules/afmongodb modules/afmongodb/ mod-afmongodb mod-mongodb: \
	modules/afmongodb/libafmongodb.la

include modules/afmongodb/tests/Makefile.am
else
modules/afmongodb modules/afmongodb/ mod-afmongodb mod-mongodb:
endif

if LIBMONGO_INTERNAL

EXTRA_DIST					+=	\
	modules/afmongodb/mongo-c-driver/configure.gnu

endif

BUILT_SOURCES					+=	\
	modules/afmongodb/afmongodb-grammar.y		\
	modules/afmongodb/afmongodb-grammar.c		\
	modules/afmongodb/afmongodb-grammar.h

EXTRA_DIST					+=	\
	modules/afmongodb/afmongodb-grammar.ym

.PHONY: modules/afmongodb/ mod-afmongodb mod-mongodb
