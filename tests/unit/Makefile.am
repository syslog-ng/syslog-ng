

AM_CFLAGS = -I$(top_srcdir)/lib -I$(top_srcdir)/modules -I../../lib -I../../modules
LIBS = @DEPS_LIBS@
LDADD = $(top_builddir)/lib/libsyslog-ng.la $(top_builddir)/modules/syslogformat/libsyslogformat.la

check_PROGRAMS = test_persist_state test_resolve_pwgr test_nvtable test_msgsdata \
	test_radix test_logqueue test_matcher test_clone_logmsg test_csvparser \
	test_serialize test_msgparse test_template test_template_speed test_zone \
	test_filters test_dnscache test_findeom test_findcrlf test_tags \
	test_patterndb_v2 test_patterndb_v3 test_patternize test_logwriter

test_msgparse_SOURCES = test_msgparse.c libtest.c
test_template_SOURCES = test_template.c libtest.c
test_template_LDADD = $(LDADD) $(top_builddir)/modules/basicfuncs/libbasicfuncs.la $(top_builddir)/modules/convertfuncs/libconvertfuncs.la
test_template_speed_SOURCES = test_template_speed.c libtest.c
test_zone_SOURCES = test_zone.c  libtest.c
test_dnscache_SOURCES = test_dnscache.c  libtest.c
test_serialize_SOURCES = test_serialize.c  libtest.c
test_findeom_SOURCES = test_findeom.c  libtest.c
test_findcrlf_SOURCES = test_findcrlf.c  libtest.c
test_csvparser_SOURCES = test_csvparser.c  libtest.c
test_csvparser_LDADD = $(LDADD) $(top_builddir)/modules/csvparser/libcsvparser.la
test_clone_logmsg_SOURCES = test_clone_logmsg.c libtest.c
test_matcher_SOURCES = test_matcher.c libtest.c
test_filters_SOURCES = test_filters.c libtest.c
test_logqueue_SOURCES = test_logqueue.c
test_radix_SOURCES = test_radix.c
test_msgsdata_SOURCES = test_msgsdata.c
test_tags_SOURCES = test_tags.c
test_nvtable_SOURCES = test_nvtable.c
test_logwriter_SOURCES = test_logwriter.c
test_resolve_pwgr_SOURCES = test_resolve_pwgr.c

test_radix_LDADD = $(LDADD) $(top_builddir)/modules/dbparser/libdbparser.la
test_patterndb_v2_SOURCES = test_patterndb_v2.c
test_patterndb_v2_LDADD = $(LDADD) $(top_builddir)/modules/dbparser/libdbparser.la
test_patterndb_v3_SOURCES = test_patterndb_v3.c
test_patterndb_v3_LDADD = $(LDADD) $(top_builddir)/modules/dbparser/libdbparser.la
test_patternize_LDADD = $(LDADD) $(top_builddir)/modules/dbparser/libdbparser.la
test_patternize_SOURCES = test_patternize.c
test_persist_state_SOURCES = test_persist_state.c

TESTS = $(check_PROGRAMS)

