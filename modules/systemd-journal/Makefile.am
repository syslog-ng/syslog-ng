moduledir = @moduledir@
module_LTLIBRARIES = libsdjournal.la
export top_srcdir

SUBDIRS = . tests

libsdjournal_la_SOURCES = \
	systemd-journal.c \
	systemd-journal.h \
	systemd-journal-grammar.y \
	systemd-journal-parser.c \
	systemd-journal-parser.h \
	systemd-journal-plugin.c \
	journal-reader.c \
	journal-reader.h \
	journald-subsystem.c \
	journald-subsystem.h \
	journald-helper.c


BUILT_SOURCES = systemd-journal-grammar.y \
		systemd-journal-grammar.c \
		systemd-journal-grammar.h

libsdjournal_la_CFLAGS = $(AM_CPPFLAGS)
libsdjournal_la_LIBADD = $(MODULE_DEPS_LIBS)
libsdjournal_la_LDFLAGS = $(MODULE_LDFLAGS)

include $(top_srcdir)/build/lex-rules.am

EXTRA_DIST = $(BUILT_SOURCES) systemd-journal-grammar.ym

install:
	$(MAKE) $(AM_MAKEFLAGS) all
	$(MAKE) $(AM_MAKEFLAGS) install-am

uninstall:
	$(MAKE) $(AM_MAKEFLAGS) uninstall-am
