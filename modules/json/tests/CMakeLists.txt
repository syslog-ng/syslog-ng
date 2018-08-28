add_unit_test(LIBTEST CRITERION TARGET test_format_json
  DEPENDS syslogformat json-plugin ${JSONC_LIBRARY})
if (${JSONC_INTERNAL})
  add_dependencies(test_format_json JSONC)
endif()

add_unit_test(LIBTEST CRITERION TARGET test_json_parser
  INCLUDES "${JSON_INCLUDE_DIR}"
  DEPENDS json-plugin ${JSONC_LIBRARY})
if (${JSONC_INTERNAL})
  add_dependencies(test_json_parser JSONC)
endif()

add_unit_test(LIBTEST CRITERION TARGET test_dot_notation
  INCLUDES "${JSON_INCLUDE_DIR}" "${JSONC_INCLUDE_DIR}"
  DEPENDS json-plugin ${JSONC_LIBRARY})
if (${JSONC_INTERNAL})
  add_dependencies(test_dot_notation JSONC)
endif()
