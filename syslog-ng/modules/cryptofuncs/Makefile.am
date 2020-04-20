module_LTLIBRARIES					+=	\
	modules/cryptofuncs/libcryptofuncs.la

EXTRA_DIST += modules/cryptofuncs/CMakeLists.txt

modules_cryptofuncs_libcryptofuncs_la_SOURCES		=	\
	modules/cryptofuncs/cryptofuncs.c
modules_cryptofuncs_libcryptofuncs_la_LIBADD		=	\
	$(MODULE_DEPS_LIBS) $(OPENSSL_LIBS)
modules_cryptofuncs_libcryptofuncs_la_LDFLAGS		=	\
	$(MODULE_LDFLAGS) $(OPENSSL_CFLAGS)
modules_cryptofuncs_libcryptofuncs_la_DEPENDENCIES	=	\
	$(MODULE_DEPS_LIBS)

modules/cryptofuncs modules/cryptofuncs/ mod-cryptofuncs: modules/cryptofuncs/libcryptofuncs.la
.PHONY: modules/cryptofuncs/ mod-cryptofuncs

include modules/cryptofuncs/tests/Makefile.am
