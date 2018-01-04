module_LTLIBRARIES += modules/add-contextual-data-filter/libadd-contextual-data-filter.la

modules_add_contextual_data_filter_libadd_contextual_data_filter_la_SOURCES	=		\
	modules/add-contextual-data-filter/add-contextual-data-filter.h			\
	modules/add-contextual-data-filter/add-contextual-data-filter.c			\
	modules/add-contextual-data-filter/add-contextual-data-filter-grammar.y 		\
	modules/add-contextual-data-filter/add-contextual-data-filter-parser.h 		\
	modules/add-contextual-data-filter/add-contextual-data-filter-parser.c 		\
	modules/add-contextual-data-filter/add-contextual-data-filter-plugin.c

modules_add_contextual_data_filter_libadd_contextual_data_filter_la_CPPFLAGS	=		\
	$(AM_CPPFLAGS)								                                \
	-I$(top_srcdir)/modules/add-contextual-data-filter						\
	-I$(top_builddir)/modules/add-contextual-data-filter                                           \
	-I$(top_srcdir)/modules/add-contextual-data                

modules_add_contextual_data_filter_libadd_contextual_data_filter_la_LIBADD	=		\
	$(MODULE_DEPS_LIBS)
modules_add_contextual_data_filter_libadd_contextual_data_filter_la_LDFLAGS	=		\
	$(MODULE_LDFLAGS)
modules_add_contextual_data_filter_libadd_contextual_data_filter_la_DEPENDENCIES	=	\
	$(MODULE_DEPS_LIBS)

BUILT_SOURCES					+=				\
	modules/add-contextual-data-filter/add-contextual-data-filter-grammar.y			\
	modules/add-contextual-data-filter/add-contextual-data-filter-grammar.c			\
	modules/add-contextual-data-filter/add-contextual-data-filter-grammar.h

EXTRA_DIST					+=				\
	modules/add-contextual-data-filter/add-contextual-data-filter-grammar.ym

modules/add-contextual-data-filter modules/add-contextual-data-filter/ mod-add-contextual-data-filter:	\
	modules/add-contextual-data-filter/libadd_contextual_data_filter.la


.PHONY: modules/add-contextual-data-filter/ mod-add-contextual-data-filter

include modules/add-contextual-data-filter/tests/Makefile.am
