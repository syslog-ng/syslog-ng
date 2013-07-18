
# we need the dotdot version of the include path to find generated headers
AM_CFLAGS = -I$(top_srcdir)/lib -I$(top_srcdir)/modules -I../lib -I../modules

noinst_LIBRARIES = libsyslog-ng-test.a

libsyslog_ng_test_a_SOURCES = \
	libtest.c		\
	testutils.c		\
	testutils.h		\
	msg_parse_lib.c		\
	msg_parse_lib.h		\
	template_lib.c		\
	template_lib.h		\
	mock-transport.c	\
	mock-transport.h \
  libtest_memreader.c \
  libtest_memreader.h
