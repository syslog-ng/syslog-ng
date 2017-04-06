bin_PROGRAMS			+= tests/loggen/loggen

tests_loggen_loggen_CPPFLAGS	= -I$(top_srcdir)/lib
tests_loggen_loggen_SOURCES	=	\
	tests/loggen/loggen.c		\
	lib/crypto.c			\
	lib/compat/openssl_support.c

tests_loggen_loggen_LDADD	= @BASE_LIBS@ @OPENSSL_LIBS@ @GLIB_LIBS@
