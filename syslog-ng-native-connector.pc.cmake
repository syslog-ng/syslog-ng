prefix=${CMAKE_INSTALL_PREFIX}
exec_prefix=${PKG_CONFIG_EXEC_PREFIX}
libdir=${PKG_CONFIG_LIBDIR}

Name: libsyslog-ng-native-connector
Description: Common connector for a native syslog-ng module
Requires: syslog-ng
Version: 0.1.0
Libs: ${PKG_CONFIG_LIBS}
