if ENABLE_PACCT

module_LTLIBRARIES				+= modules/pacctformat/libpacctformat.la
modules_pacctformat_libpacctformat_la_SOURCES	=	\
	modules/pacctformat/pacct-format.c		\
	modules/pacctformat/pacct-format.h		\
	modules/pacctformat/pacct-format-plugin.c

modules_pacctformat_libpacctformat_la_CPPFLAGS	=	\
	$(AM_CPPFLAGS)
modules_pacctformat_libpacctformat_la_LIBADD	=	\
	$(MODULE_DEPS_LIBS)
modules_pacctformat_libpacctformat_la_LDFLAGS	=	\
	$(MODULE_LDFLAGS)
modules_pacctformat_libpacctformat_la_DEPENDENCIES=	\
	$(MODULE_DEPS_LIBS)

modules/pacctformat modules/pacctformat/ mod-pacctformat: \
	modules/pacctformat/libpacctformat.la
else
modules/pacctformat modules/pacctformat/ mod-pacctformat:
endif

EXTRA_DIST += modules/pacctformat/CMakeLists.txt

.PHONY: modules/pacctformat/ mod-pacctformat
