# Package Information for pkg-config
prefix=@CMAKE_INSTALL_PREFIX@
exec_prefix=@PKG_CONFIG_EXEC_PREFIX@
datarootdir=@PKG_CONFIG_DATAROOTDIR@
datadir=@PKG_CONFIG_DATADIR@
libdir=@PKG_CONFIG_LIBDIR@
includedir=@PKG_CONFIG_INCLUDEDIR@
toolsdir=@PKG_CONFIG_TOOLSDIR@
moduledir=@PKG_CONFIG_MODULEDIR@
config_includedir=@PKG_CONFIG_CONFIG_INCLUDEDIR@
scldir=@PKG_CONFIG_SCLDIR@
ivykis=@PKG_CONFIG_IVYKIS@

Name: syslog-ng-dev
Description: Dev package for syslog-ng module
Version: @PKG_CONFIG_PACKAGE_VERSION@
Requires: glib-2.0 eventlog ivykis gthread-2.0 gmodule-2.0
Libs: -L${libdir} -lsyslog-ng -levtlog
Cflags: -I${includedir}/syslog-ng
