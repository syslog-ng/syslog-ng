add_unit_test(CRITERION TARGET test_afsnmp_dest DEPENDS afsnmp)
add_unit_test(CRITERION TARGET test_varbindlist_scanner DEPENDS afsnmp)
add_unit_test(LIBTEST CRITERION TARGET test_snmptrapd_parser DEPENDS afsnmp)
