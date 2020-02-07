add_unit_test(CRITERION TARGET test_http DEPENDS http)
add_unit_test(LIBTEST CRITERION TARGET test_http-loadbalancer DEPENDS http)
add_unit_test(CRITERION TARGET test_http-response_handlers DEPENDS http)
add_unit_test(CRITERION TARGET test_http-signal_slot DEPENDS http)
