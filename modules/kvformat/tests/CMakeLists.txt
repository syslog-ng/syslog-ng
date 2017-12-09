add_unit_test(LIBTEST TARGET test_format_welf DEPENDS syslogformat kvformat)
add_unit_test(LIBTEST TARGET test_linux_audit_scanner DEPENDS kvformat)
add_unit_test(LIBTEST TARGET test_kv_parser DEPENDS kvformat)
