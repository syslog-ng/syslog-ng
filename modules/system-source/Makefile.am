module_LTLIBRARIES					+= modules/system-source/libsystem-source.la

modules_system_source_libsystem_source_la_SOURCES	=	\
	modules/system-source/system-source.c
modules_system_source_libsystem_source_la_CPPFLAGS	=	\
	$(AM_CPPFLAGS) $(libsystemd_CFLAGS)
modules_system_source_libsystem_source_la_LIBADD	=	\
	$(MODULE_DEPS_LIBS) $(libsystemd_LIBS)
modules_system_source_libsystem_source_la_LDFLAGS	=	\
	$(MODULE_LDFLAGS)
modules_system_source_libsystem_source_la_DEPENDENCIES	=	\
	$(MODULE_DEPS_LIBS)

tools_SCRIPTS						+=	\
	modules/system-source/system-expand.sh

EXTRA_DIST						+=	\
	modules/system-source/system-expand.sh	\
	modules/system-source/CMakeLists.txt

modules/system-source modules/system-source/ mod-system-source: \
	modules/system-source/libsystem-source.la
.PHONY: modules/system-source/ mod-system-source
