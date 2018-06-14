if ENABLE_SMTP
module_LTLIBRARIES			+= modules/afsmtp/libafsmtp.la

modules_afsmtp_libafsmtp_la_CFLAGS	=	\
    $(AM_CFLAGS) \
	$(LIBESMTP_CFLAGS)			\
	-I$(top_srcdir)/modules/afsmtp		\
	-I$(top_builddir)/modules/afsmtp
modules_afsmtp_libafsmtp_la_SOURCES	=	\
	modules/afsmtp/afsmtp-grammar.y		\
	modules/afsmtp/afsmtp.c			\
	modules/afsmtp/afsmtp.h			\
	modules/afsmtp/afsmtp-parser.c		\
	modules/afsmtp/afsmtp-parser.h
modules_afsmtp_libafsmtp_la_LIBADD	=	\
	$(MODULE_DEPS_LIBS) $(LIBESMTP_LIBS)
modules_afsmtp_libafsmtp_la_LDFLAGS	=	\
	$(MODULE_LDFLAGS)
modules_afsmtp_libafsmtp_la_DEPENDENCIES=	\
	$(MODULE_DEPS_LIBS)

modules/afsmtp modules/afsmtp/ mod-afsmtp mod-smtp: \
	modules/afsmtp/libafsmtp.la
else
modules/afsmtp modules/afsmtp/ mod-afsmtp mod-smtp:
endif

BUILT_SOURCES				+=	\
	modules/afsmtp/afsmtp-grammar.y		\
	modules/afsmtp/afsmtp-grammar.c		\
	modules/afsmtp/afsmtp-grammar.h
EXTRA_DIST				+=	\
	modules/afsmtp/afsmtp-grammar.ym	\
	modules/afsmtp/CMakeLists.txt

.PHONY: modules/afsmtp/ mod-afsmtp mod-smtp
