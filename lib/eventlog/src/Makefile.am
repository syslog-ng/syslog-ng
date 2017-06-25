
lib_LTLIBRARIES += lib/eventlog/src/libevtlog.la

lib_eventlog_src_libevtlog_la_SOURCES = \
		lib/eventlog/src/evtrec.c \
		lib/eventlog/src/evtfmt.c \
		lib/eventlog/src/evtout.c \
		lib/eventlog/src/evtstr.c \
		lib/eventlog/src/evtctx.c \
		lib/eventlog/src/evttags.c \
		lib/eventlog/src/evtsyslog.c

lib_eventlog_src_libevtlog_la_LDFLAGS = -no-undefined -release ${LSNG_RELEASE} \
					-version-info ${LSNG_CURRENT}:${LSNG_REVISION}:${LSNG_AGE}

lib_eventlog_src_libevtlog_la_CFLAGS = $(AM_CFLAGS)


noinst_HEADERS = lib/eventlog/src/evt_internals.h

pkginclude_HEADERS += \
		lib/eventlog/src/evtmaps.h \
		lib/eventlog/src/evtlog.h
