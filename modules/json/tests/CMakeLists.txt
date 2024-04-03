add_unit_test(LIBTEST CRITERION TARGET test_format_json
  DEPENDS syslogformat json-plugin ${JSONC_LIBRARY})

add_unit_test(LIBTEST CRITERION TARGET test_filterx_format_json
  DEPENDS syslogformat json-plugin ${JSONC_LIBRARY})

add_unit_test(LIBTEST CRITERION TARGET test_json_parser
  INCLUDES "${JSON_INCLUDE_DIR}"
  DEPENDS json-plugin ${JSONC_LIBRARY})

add_unit_test(LIBTEST CRITERION TARGET test_dot_notation
  INCLUDES "${JSON_INCLUDE_DIR}" "${JSONC_INCLUDE_DIR}"
  DEPENDS json-plugin ${JSONC_LIBRARY})
