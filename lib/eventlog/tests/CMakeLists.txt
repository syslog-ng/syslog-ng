add_unit_test(TARGET test_evtrec
  INCLUDES "${Eventlog_INCLUDE_DIR}"
  SOURCES test_evtrec.c)

add_unit_test(TARGET test_evtfmt
  INCLUDES "${Eventlog_INCLUDE_DIR}"
  SOURCES test_evtfmt.c)

add_unit_test(CRITERION TARGET test_evt_tag_mem
  INCLUDES "${Eventlog_INCLUDE_DIR}"
  SOURCES test_evt_tag_mem.c)

add_unit_test(TARGET test_evtsyslog
  INCLUDES "${Eventlog_INCLUDE_DIR}"
  SOURCES test_evtsyslog.c)

add_unit_test(TARGET test_evtsyslog_macros
  INCLUDES "${Eventlog_INCLUDE_DIR}"
  SOURCES test_evtsyslog.c)
target_compile_definitions(test_evtsyslog_macros PRIVATE EVENTLOG_SYSLOG_MACROS=1)
