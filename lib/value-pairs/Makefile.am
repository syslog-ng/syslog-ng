value_pairsincludedir			= ${pkgincludedir}/value-pairs

EXTRA_DIST += lib/value-pairs/CMakeLists.txt	\
	lib/value-pairs/tests/CMakeLists.txt

value_pairsinclude_HEADERS = \
	lib/value-pairs/value-pairs.h		\
	lib/value-pairs/transforms.h		\
	lib/value-pairs/cmdline.h		\
	lib/value-pairs/evttag.h

value_pairs_sources = \
	lib/value-pairs/value-pairs.c		\
	lib/value-pairs/transforms.c		\
	lib/value-pairs/cmdline.c		\
	lib/value-pairs/evttag.c

include lib/value-pairs/tests/Makefile.am
