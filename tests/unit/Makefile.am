

AM_CFLAGS = -I$(top_srcdir)/src
LDADD = $(top_builddir)/src/libsyslog-ng.a

check_PROGRAMS = test_msgparse test_template test_zone

test_msgparse_SOURCES = test_msgparse.c

test_template_SOURCES = test_template.c

test_zone_SOURCES = test_zone.c

TESTS = test_msgparse test_template test_zone
