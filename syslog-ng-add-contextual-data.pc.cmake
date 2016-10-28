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

Name: libsyslog-ng-add-contextual-data
Description: add-contextual-data lib
Required: syslog-ng
Version: 0.1.0
Libs: -L${moduledir} -ladd-contextual-data
Cflags: -I${includedir}/syslog-ng/modules/add-contextual-data
