set(prefix ${CMAKE_INSTALL_PREFIX})
set(exec_prefix ${SYSLOG_NG_PATH_EXECPREFIX})
set(bindir "\${exec_prefix}/bin")
set(sysconfdir ${SYSLOG_NG_PATH_SYSCONFDIR})
set(localstatedir ${SYSLOG_NG_PATH_LOCALSTATEDIR})

configure_file (${CMAKE_CURRENT_SOURCE_DIR}/update-patterndb.in ${CMAKE_CURRENT_BINARY_DIR}/update-patterndb)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/update-patterndb DESTINATION bin)
install(DIRECTORY DESTINATION etc/patterndb.d)
