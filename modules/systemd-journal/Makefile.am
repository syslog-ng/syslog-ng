if ENABLE_JOURNALD
module_LTLIBRARIES += modules/systemd-journal/libsdjournal.la
modules_systemd_journal_libsdjournal_la_SOURCES = \
	modules/systemd-journal/systemd-journal.c \
	modules/systemd-journal/systemd-journal.h \
	modules/systemd-journal/systemd-journal-grammar.y \
	modules/systemd-journal/systemd-journal-parser.c \
	modules/systemd-journal/systemd-journal-parser.h \
	modules/systemd-journal/systemd-journal-plugin.c \
	modules/systemd-journal/journal-reader.c \
	modules/systemd-journal/journal-reader.h \
	modules/systemd-journal/journald-subsystem.c \
	modules/systemd-journal/journald-subsystem.h \
	modules/systemd-journal/journald-helper.c

modules_systemd_journal_libsdjournal_la_CPPFLAGS = $(AM_CPPFLAGS) $(libsystemd_CFLAGS) -I$(top_srcdir)/modules/systemd-journal -I$(top_builddir)/modules/systemd-journal
modules_systemd_journal_libsdjournal_la_LIBADD = $(MODULE_DEPS_LIBS) $(libsystemd_LIBS)
modules_systemd_journal_libsdjournal_la_LDFLAGS = $(MODULE_LDFLAGS)
modules_systemd_journal_libsdjournal_la_DEPENDENCIES = $(MODULE_DEPS_LIBS)

include modules/systemd-journal/tests/Makefile.am
modules/systemd-journal mod-systemd-journal: modules/systemd-journal/libsdjournal.la
else
modules/systemd-journal mod-systemd-journal:
endif

BUILT_SOURCES += modules/systemd-journal/systemd-journal-grammar.y \
		 modules/systemd-journal/systemd-journal-grammar.c \
		 modules/systemd-journal/systemd-journal-grammar.h

EXTRA_DIST +=	\
	modules/systemd-journal/CMakeLists.txt	\
	modules/systemd-journal/systemd-journal-grammar.ym

.PHONY: modules/systemd-journal mod-systemd-journal
