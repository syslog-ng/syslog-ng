
noinst_PROGRAMS = test_msgparse test_template

AM_CFLAGS = -I$(top_srcdir)/src
test_msgparse_SOURCES = test_msgparse.c
test_msgparse_LDADD = $(top_builddir)/src/libsyslog-ng.a

test_template_SOURCES = test_template.c
test_template_LDADD = $(top_builddir)/src/libsyslog-ng.a

TESTS = test_msgparse test_template
