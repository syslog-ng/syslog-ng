module_LTLIBRARIES					+= modules/system-source/libsystem-source.la

modules_system_source_libsystem_source_la_SOURCES	=	\
	modules/system-source/system-source.c
modules_system_source_libsystem_source_la_CPPFLAGS	=	\
	$(AM_CPPFLAGS) $(libsystemd_daemon_CFLAGS)
modules_system_source_libsystem_source_la_LIBADD	=	\
	$(MODULE_DEPS_LIBS) $(libsystemd_daemon_LIBS)
modules_system_source_libsystem_source_la_LDFLAGS	=	\
	$(MODULE_LDFLAGS)
modules_system_source_libsystem_source_la_DEPENDENCIES	=	\
	$(MODULE_DEPS_LIBS)

modules/system-source modules/system-source/ mod-system-source: \
	modules/system-source/libsystem-source.la
.PHONY: modules/system-source/ mod-system-source
