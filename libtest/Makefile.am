libtestdir			    =	\
	$(pkglibdir)/libtest
libtestincludedir		    =	\
	$(pkgincludedir)/libtest

EXTRA_DIST += libtest/CMakeLists.txt

libtest_LIBRARIES		    =	\
	libtest/libsyslog-ng-test.a

libtest_libsyslog_ng_test_a_SOURCES =   \
	libtest/config_parse_lib.c	\
	libtest/config_parse_lib.h	\
	libtest/cr_template.c		\
	libtest/grab-logging.c		\
	libtest/grab-logging.h		\
	libtest/libtest.c		\
	libtest/mock-cfg-parser.c	\
	libtest/mock-cfg-parser.h	\
	libtest/mock-transport.c	\
	libtest/mock-transport.h	\
	libtest/msg_parse_lib.c		\
	libtest/msg_parse_lib.h		\
	libtest/persist_lib.c		\
	libtest/persist_lib.h		\
	libtest/proto_lib.c		\
	libtest/proto_lib.h		\
	libtest/queue_utils_lib.c	\
	libtest/queue_utils_lib.h	\
	libtest/stopwatch.c		\
	libtest/stopwatch.h		\
	libtest/testutils.c		\
	libtest/testutils.h

libtestinclude_HEADERS		    =	\
	libtest/cr_template.h		\
	libtest/grab-logging.h		\
	libtest/mock-transport.h	\
	libtest/msg_parse_lib.h		\
	libtest/persist_lib.h		\
	libtest/proto_lib.h		\
	libtest/testutils.h

pkgconfig_DATA			   +=	\
	libtest/syslog-ng-test.pc

LIBTEST_LIBS = $(top_builddir)/libtest/libsyslog-ng-test.a

include libtest/tests/Makefile.am
