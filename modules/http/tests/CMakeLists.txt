add_unit_test(CRITERION TARGET test_http DEPENDS http)
add_unit_test(LIBTEST CRITERION TARGET test_http-loadbalancer DEPENDS http)
