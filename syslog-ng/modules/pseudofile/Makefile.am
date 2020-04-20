module_LTLIBRARIES			+= modules/pseudofile/libpseudofile.la
modules_pseudofile_libpseudofile_la_SOURCES	=	\
	modules/pseudofile/pseudofile.c			\
	modules/pseudofile/pseudofile.h			\
	modules/pseudofile/pseudofile-grammar.y		\
	modules/pseudofile/pseudofile-parser.c		\
	modules/pseudofile/pseudofile-parser.h		\
	modules/pseudofile/pseudofile-plugin.c

modules_pseudofile_libpseudofile_la_CPPFLAGS	=	\
	$(AM_CPPFLAGS)				\
	-I$(top_srcdir)/modules/pseudofile		\
	-I$(top_builddir)/modules/pseudofile
modules_pseudofile_libpseudofile_la_LIBADD	=	\
	$(MODULE_DEPS_LIBS)
modules_pseudofile_libpseudofile_la_LDFLAGS	=	\
	$(MODULE_LDFLAGS)
modules_pseudofile_libpseudofile_la_DEPENDENCIES=	\
	$(MODULE_DEPS_LIBS)

BUILT_SOURCES				+=	\
	modules/pseudofile/pseudofile-grammar.y		\
	modules/pseudofile/pseudofile-grammar.c		\
	modules/pseudofile/pseudofile-grammar.h
EXTRA_DIST				+=	\
	modules/pseudofile/pseudofile-grammar.ym	\
	modules/pseudofile/CMakeLists.txt

modules/pseudofile modules/pseudofile/ mod-pseudofile: \
	modules/pseudofile/libpseudofile.la
.PHONY: modules/pseudofile/ mod-pseudofile
