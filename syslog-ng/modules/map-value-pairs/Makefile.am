module_LTLIBRARIES				+= modules/map-value-pairs/libmap-value-pairs.la

modules_map_value_pairs_libmap_value_pairs_la_CPPFLAGS		= \
    $(AM_CPPFLAGS) \
	-I$(top_srcdir)/modules/map-value-pairs		  \
	-I$(top_builddir)/modules/map-value-pairs

modules_map_value_pairs_libmap_value_pairs_la_LIBADD		= \
	$(MODULE_DEPS_LIBS)

modules_map_value_pairs_libmap_value_pairs_la_SOURCES		= \
	modules/map-value-pairs/map-value-pairs.c		 	  \
	modules/map-value-pairs/map-value-pairs.h		  	  \
	modules/map-value-pairs/map-value-pairs-plugin.c		  \
	modules/map-value-pairs/map-value-pairs-grammar.y		  \
	modules/map-value-pairs/map-value-pairs-parser.c		  \
	modules/map-value-pairs/map-value-pairs-parser.h


modules_map_value_pairs_libmap_value_pairs_la_LDFLAGS		 = \
	$(MODULE_LDFLAGS)

modules_map_value_pairs_libmap_value_pairs_la_DEPENDENCIES=	\
	$(MODULE_DEPS_LIBS)

BUILT_SOURCES					+= \
	modules/map-value-pairs/map-value-pairs-grammar.y		   \
	modules/map-value-pairs/map-value-pairs-grammar.c		   \
	modules/map-value-pairs/map-value-pairs-grammar.h

modules/map-value-pairs mod-map-value-pairs: modules/map-value-pairs/libmod-map-value-pairs.la

EXTRA_DIST					+= \
	modules/map-value-pairs/map-value-pairs-grammar.ym \
	modules/map-value-pairs/CMakeLists.txt

.PHONY: modules/map-value-pairs mod-map-value-pairs
