check_PROGRAMS						+= \
	${modules_file_rotation_tests_TESTS}

EXTRA_DIST += modules/file-rotation/tests/CMakeLists.txt

modules_file_rotation_tests_TESTS 				= \
	modules/file-rotation/tests/test_file_rotation

modules_file_rotation_tests_test_file_rotation_CFLAGS  = $(TEST_CFLAGS) -I$(top_srcdir)/modules/file-rotation
modules_file_rotation_tests_test_file_rotation_LDADD   = $(TEST_LDADD) \
	-dlpreopen $(top_builddir)/modules/file-rotation/libfile-rotation.la
