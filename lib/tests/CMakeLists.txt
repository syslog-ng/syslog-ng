add_unit_test(LIBTEST TARGET test_cfg_lexer_subst)
add_unit_test(LIBTEST TARGET test_cfg_tree)
add_unit_test(LIBTEST TARGET test_type_hints)
add_unit_test(LIBTEST TARGET test_parse_number)
add_unit_test(LIBTEST TARGET test_reloc)
add_unit_test(LIBTEST TARGET test_hostname)
add_unit_test(LIBTEST TARGET test_rcptid)
add_unit_test(LIBTEST TARGET test_lexer)
add_unit_test(LIBTEST TARGET test_str_format)
add_unit_test(LIBTEST TARGET test_string_list)
add_unit_test(LIBTEST CRITERION TARGET test_runid)
add_unit_test(LIBTEST TARGET test_pathutils)
add_unit_test(LIBTEST TARGET test_utf8utils)
add_unit_test(LIBTEST TARGET test_userdb)
add_unit_test(LIBTEST TARGET test_str-utils)

add_unit_test(CRITERION TARGET test_cache)
add_unit_test(CRITERION TARGET test_scratch_buffers)
add_unit_test(CRITERION TARGET test_timeutils)

SET_DIRECTORY_PROPERTIES(PROPERTIES
  ADDITIONAL_MAKE_CLEAN_FILES
  "test_values.persist;test_values.persist-;test_run_id.persist;test_run_id.persist-")
