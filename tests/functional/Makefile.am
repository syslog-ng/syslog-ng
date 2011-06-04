
export top_srcdir
export top_builddir
dbd_dir = $(shell echo $(exec_prefix)/lib/dbd)
export dbd_dir

EXTRA_DIST = func_test.py control.py globals.py log.py messagecheck.py messagegen.py \
	ssl.crt ssl.key rnd.in \
	test_file_source.py test_filters.py test_input_drivers.py test_performance.py test_sql.py

TESTS = func_test.py

rnd: rnd.in
	cp $< $@
