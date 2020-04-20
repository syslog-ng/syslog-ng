add_unit_test(TARGET test_evtrec
  INCLUDES "${Eventlog_INCLUDE_DIR}"
  SOURCES evtrec.c)

add_unit_test(TARGET test_evtfmt
  INCLUDES "${Eventlog_INCLUDE_DIR}"
  SOURCES evtfmt.c)

add_unit_test(TARGET test_evtsyslog
  INCLUDES "${Eventlog_INCLUDE_DIR}"
  SOURCES evtsyslog.c)

add_unit_test(TARGET test_evtsyslog_macros
  INCLUDES "${Eventlog_INCLUDE_DIR}"
  SOURCES evtsyslog.c)
target_compile_definitions(test_evtsyslog_macros PRIVATE EVENTLOG_SYSLOG_MACROS=1)
