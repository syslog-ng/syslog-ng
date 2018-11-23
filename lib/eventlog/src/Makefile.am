
lib_LTLIBRARIES += lib/eventlog/src/libevtlog.la

EXTRA_DIST +=	\
	lib/eventlog/AUTHORS	\
	lib/eventlog/ChangeLog	\
	lib/eventlog/ChangeLog.0	\
	lib/eventlog/CMakeLists.txt	\
	lib/eventlog/COPYING	\
	lib/eventlog/CREDITS	\
	lib/eventlog/doc	\
	lib/eventlog/doc/API.txt	\
	lib/eventlog/doc/DESIGN.txt	\
	lib/eventlog/NEWS

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

lib_eventlog_src_libevtlog_la_LIBADD = @GLIB_LIBS@


noinst_HEADERS = lib/eventlog/src/evt_internals.h

pkginclude_HEADERS += \
		lib/eventlog/src/evtmaps.h \
		lib/eventlog/src/evtlog.h
