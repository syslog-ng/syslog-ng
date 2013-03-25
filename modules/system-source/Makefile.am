moduledir = @moduledir@
AM_CPPFLAGS = -I$(top_srcdir)/lib -I../../lib
export top_srcdir

module_LTLIBRARIES := libsystem-source.la
libsystem_source_la_SOURCES = system-source.c
libsystem_source_la_CPPFLAGS = $(AM_CPPFLAGS) $(libsystemd_daemon_CFLAGS)
libsystem_source_la_LIBADD = $(MODULE_DEPS_LIBS) $(libsystemd_daemon_LIBS)
libsystem_source_la_LDFLAGS = $(MODULE_LDFLAGS)
