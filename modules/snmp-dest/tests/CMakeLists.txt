add_unit_test(CRITERION TARGET test_snmp_dest DEPENDS snmpdest)
include_directories(test_snmp_dest PRIVATE
  ${PROJECT_SOURCE_DIR}/modules/snmp-dest
  )
