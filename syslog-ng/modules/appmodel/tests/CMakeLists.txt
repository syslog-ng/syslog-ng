add_unit_test(LIBTEST CRITERION TARGET test_appmodel INCLUDES "${APPMODEL_INCLUDE_DIR}" DEPENDS appmodel)
add_unit_test(CRITERION TARGET test_appmodel_context INCLUDES "${APPMODEL_INCLUDE_DIR}" DEPENDS appmodel)
add_unit_test(LIBTEST CRITERION TARGET test_app_parser_generator INCLUDES "${APPMODEL_INCLUDE_DIR}" DEPENDS appmodel)
add_unit_test(CRITERION TARGET test_application INCLUDES "${APPMODEL_INCLUDE_DIR}" DEPENDS appmodel)
