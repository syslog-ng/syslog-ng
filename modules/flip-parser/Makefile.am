module_LTLIBRARIES			+= modules/flip-parser/libflip-parser.la

modules_flip_parser_libflip_parser_la_SOURCES	=	\
	modules/flip-parser/flip-parser.c			\
	modules/flip-parser/flip-parser.h			\
	modules/flip-parser/flip-parser-grammar.y		\
	modules/flip-parser/flip-parser-parser.c		\
	modules/flip-parser/flip-parser-parser.h		\
	modules/flip-parser/flip-parser-plugin.c

modules_flip_parser_libflip_parser_la_CPPFLAGS	=	\
	$(AM_CPPFLAGS)				\
	-I$(top_srcdir)/modules/flip-parser		\
	-I$(top_builddir)/modules/flip-parser
modules_flip_parser_libflip_parser_la_LIBADD	=	\
	$(MODULE_DEPS_LIBS)
modules_flip_parser_libflip_parser_la_LDFLAGS	=	\
	$(MODULE_LDFLAGS)
modules_flip_parser_libflip_parser_la_DEPENDENCIES	=	\
	$(MODULE_DEPS_LIBS)

modules/flip-parser modules/flip-parser mod-flip-parser: modules/flip-parser/libflip-parser.la
.PHONY: modules/flip-parser/ mod-flip-parser

BUILT_SOURCES       +=      \
  modules/flip-parser/flip-parser-grammar.y       \
  modules/flip-parser/flip-parser-grammar.c       \
  modules/flip-parser/flip-parser-grammar.h

EXTRA_DIST        +=      \
  modules/flip-parser/flip-parser-grammar.ym	\
  modules/flip-parser/CMakeLists.txt

