add_unit_test(LIBTEST CRITERION
  TARGET test_python_logmsg
  INCLUDES "${PYTHON_INCLUDE_DIR}" "${PYTHON_INCLUDE_DIRS}"
  DEPENDS syslogformat mod-python "${PYTHON_LIBRARIES}")

set_property(TEST test_python_logmsg APPEND PROPERTY ENVIRONMENT "PYTHONMALLOC=malloc_debug")

add_unit_test(LIBTEST CRITERION
  TARGET test_python_template
  INCLUDES "${PYTHON_INCLUDE_DIR}" "${PYTHON_INCLUDE_DIRS}"
  DEPENDS mod-python "${PYTHON_LIBRARIES}" syslogformat)

set_property(TEST test_python_template APPEND PROPERTY ENVIRONMENT "PYTHONMALLOC=malloc_debug")

add_unit_test(LIBTEST CRITERION
  TARGET test_python_persist_name
  INCLUDES "${PYTHON_INCLUDE_DIR}" "${PYTHON_INCLUDE_DIRS}"
  DEPENDS syslogformat mod-python "${PYTHON_LIBRARIES}")

set_property(TEST test_python_persist_name APPEND PROPERTY ENVIRONMENT "PYTHONMALLOC=malloc_debug")

add_unit_test(LIBTEST CRITERION
  TARGET test_python_persist
  INCLUDES "${PYTHON_INCLUDE_DIR}" "${PYTHON_INCLUDE_DIRS}"
  DEPENDS syslogformat mod-python "${PYTHON_LIBRARIES}")

set_property(TEST test_python_persist APPEND PROPERTY ENVIRONMENT "PYTHONMALLOC=malloc_debug")

add_unit_test(LIBTEST CRITERION
  TARGET test_python_bookmark
  INCLUDES "${PYTHON_INCLUDE_DIR}" "${PYTHON_INCLUDE_DIRS}"
  DEPENDS mod-python "${PYTHON_LIBRARIES}")

set_property(TEST test_python_bookmark APPEND PROPERTY ENVIRONMENT "PYTHONMALLOC=malloc_debug")

add_unit_test(LIBTEST CRITERION
  TARGET test_python_ack_tracker
  INCLUDES "${PYTHON_INCLUDE_DIR}" "${PYTHON_INCLUDE_DIRS}"
  DEPENDS syslogformat mod-python "${PYTHON_LIBRARIES}")

set_property(TEST test_python_ack_tracker APPEND PROPERTY ENVIRONMENT "PYTHONMALLOC=malloc_debug")
