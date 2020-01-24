add_unit_test(LIBTEST
  TARGET test-mongodb-config
  INCLUDES "${AFMONGODB_INCLUDE_DIR}"
  DEPENDS afmongodb
  SOURCES test-mongodb-config
)
