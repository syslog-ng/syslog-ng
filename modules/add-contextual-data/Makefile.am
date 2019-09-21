module_LTLIBRARIES				+= 				\
	modules/add-contextual-data/libadd-contextual-data.la

EXTRA_DIST += modules/add-contextual-data/CMakeLists.txt

modules_add_contextual_data_libadd_contextual_data_la_SOURCES	=		\
	modules/add-contextual-data/add-contextual-data.c			\
	modules/add-contextual-data/add-contextual-data.h			\
	modules/add-contextual-data/contextual-data-record.h			\
	modules/add-contextual-data/contextual-data-record.c			\
	modules/add-contextual-data/contextual-data-record-scanner.h		\
	modules/add-contextual-data/contextual-data-record-scanner.c		\
	modules/add-contextual-data/add-contextual-data-grammar.y		\
	modules/add-contextual-data/add-contextual-data-parser.c		\
	modules/add-contextual-data/add-contextual-data-parser.h		\
	modules/add-contextual-data/context-info-db.h				\
	modules/add-contextual-data/context-info-db.c				\
	modules/add-contextual-data/add-contextual-data-plugin.c		\
	modules/add-contextual-data/add-contextual-data-selector.h		\
	modules/add-contextual-data/add-contextual-data-glob-selector.h		\
	modules/add-contextual-data/add-contextual-data-glob-selector.c     	\
	modules/add-contextual-data/add-contextual-data-template-selector.h	\
	modules/add-contextual-data/add-contextual-data-template-selector.c     \
	modules/add-contextual-data/add-contextual-data-filter-selector.h	\
	modules/add-contextual-data/add-contextual-data-filter-selector.c

modules_add_contextual_data_libadd_contextual_data_la_CFLAGS	=		\
	$(AM_CFLAGS)								\
	-I$(top_srcdir)/modules/add-contextual-data				\
	-I$(top_builddir)/modules/add-contextual-data

modules_add_contextual_data_libadd_contextual_data_la_LIBADD	=		\
	$(MODULE_DEPS_LIBS)
modules_add_contextual_data_libadd_contextual_data_la_LDFLAGS	=		\
	$(MODULE_LDFLAGS)
modules_add_contextual_data_libadd_contextual_data_la_DEPENDENCIES	=	\
	$(MODULE_DEPS_LIBS)

BUILT_SOURCES					+=				\
	modules/add-contextual-data/add-contextual-data-grammar.y		\
	modules/add-contextual-data/add-contextual-data-grammar.c		\
	modules/add-contextual-data/add-contextual-data-grammar.h

EXTRA_DIST					+=				\
	modules/add-contextual-data/add-contextual-data-grammar.ym

modules/add-contextual-data modules/add-contextual-data/ mod-add-contextual-data:	\
	modules/add-contextual-data/libadd_contextual_data.la
.PHONY: modules/add-contextual-data/ mod-add-contextual-data

include modules/add-contextual-data/tests/Makefile.am
