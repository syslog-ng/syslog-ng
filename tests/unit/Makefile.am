

AM_CFLAGS = -I$(top_srcdir)/src
LDADD = $(top_builddir)/src/libsyslog-ng.a

check_PROGRAMS = test_findeom test_msgparse test_template test_zone test_filters test_dnscache

test_msgparse_SOURCES = test_msgparse.c
test_template_SOURCES = test_template.c
test_zone_SOURCES = test_zone.c
test_dnscache_SOURCES = test_dnscache.c
test_findeom_SOURCES = test_findeom.c


TESTS = test_findeom test_msgparse test_template test_zone test_filters test_dnscache
