if(ENABLE_SQL)
  add_unit_test(
    CRITERION LIBTEST
    TARGET test_afsql_run_query
    SOURCES test_afsql_run_query.c ../afsql.c mock-dbi.c
    INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/.. ${LIBDBI_INCLUDE_DIRS}
    DEPENDS OpenSSL::SSL
  )
  add_unit_test(
    CRITERION LIBTEST
    TARGET test_afsql_build_insert_command
    SOURCES test_afsql_build_insert_command.c ../afsql.c mock-dbi.c
    INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/.. ${LIBDBI_INCLUDE_DIRS}
    DEPENDS OpenSSL::SSL
  )
endif()
