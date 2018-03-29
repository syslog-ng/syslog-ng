libtestdir			    =	\
	$(pkglibdir)/libtest
libtestincludedir		    =	\
	$(pkgincludedir)/libtest

libtest_LIBRARIES		    =	\
	libtest/libsyslog-ng-test.a

libtest_libsyslog_ng_test_a_SOURCES =   \
	libtest/libtest.c		\
	libtest/testutils.c		\
	libtest/testutils.h		\
	libtest/stopwatch.c		\
	libtest/stopwatch.h		\
	libtest/msg_parse_lib.c		\
	libtest/msg_parse_lib.h		\
	libtest/cr_template.c		\
	libtest/proto_lib.c		\
	libtest/proto_lib.h		\
	libtest/persist_lib.h		\
	libtest/persist_lib.c		\
	libtest/mock-transport.c	\
	libtest/mock-transport.h        \
	libtest/config_parse_lib.c      \
	libtest/config_parse_lib.h      \
	libtest/queue_utils_lib.c       \
	libtest/queue_utils_lib.h

libtestinclude_HEADERS		    =	\
	libtest/testutils.h		\
	libtest/msg_parse_lib.h		\
	libtest/cr_template.h		\
	libtest/proto_lib.h		\
	libtest/persist_lib.h		\
	libtest/mock-transport.h

pkgconfig_DATA			   +=	\
	libtest/syslog-ng-test.pc

LIBTEST_LIBS = $(top_builddir)/libtest/libsyslog-ng-test.a
