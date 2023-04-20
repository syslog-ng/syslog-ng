set(prefix ${CMAKE_INSTALL_PREFIX})
set(exec_prefix ${SYSLOG_NG_PATH_EXECPREFIX})
set(bindir "\${exec_prefix}/bin")
set(sysconfdir ${SYSLOG_NG_PATH_SYSCONFDIR})
set(localstatedir ${SYSLOG_NG_PATH_LOCALSTATEDIR})

configure_file (${CMAKE_CURRENT_SOURCE_DIR}/update-patterndb.in ${CMAKE_CURRENT_BINARY_DIR}/update-patterndb)
install(PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/update-patterndb DESTINATION bin)
install(DIRECTORY DESTINATION etc/patterndb.d)

set(python_venvdir ${SYSLOG_NG_PYTHON_VENV_DIR})
set(python_moduledir ${SYSLOG_NG_PYTHON_MODULE_DIR})
set(PYTHON ${PYTHON_EXECUTABLE})

configure_file (${CMAKE_CURRENT_SOURCE_DIR}/syslog-ng-update-virtualenv.in
                ${CMAKE_CURRENT_BINARY_DIR}/syslog-ng-update-virtualenv
                @ONLY)
install(PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/syslog-ng-update-virtualenv DESTINATION bin)
