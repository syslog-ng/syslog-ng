

AM_CFLAGS = -I$(top_srcdir)/lib -I$(top_srcdir)/modules -I../../lib -I../../modules
AM_LDFLAGS = -dlpreopen $(top_builddir)/modules/syslogformat/libsyslogformat.la  $(top_builddir)/modules/basic-proto/libbasic-proto.la
LIBS = @TOOL_DEPS_LIBS@
LDADD = $(top_builddir)/lib/libsyslog-ng.la

check_PROGRAMS = test_thread_wakeup	\
	test_resolve_pwgr		\
	test_nvtable			\
	test_msgsdata			\
	test_logqueue			\
	test_matcher			\
	test_clone_logmsg 		\
	test_csvparser 			\
	test_serialize 			\
	test_msgparse			\
	test_template			\
	test_template_speed		\
	test_filters			\
	test_dnscache			\
	test_findeom			\
	test_findcrlf			\
	test_tags			\
	test_logwriter			\
	test_serialize			\
	test_value_pairs		\
	test_filereader			\
	test_multiline			\
	test_msgserialize_benchmark	\
	test_msgserialize_tags		\
	test_msgformat_upgrade \
	test_rcptid


if IS_HPUX
check_PROGRAMS += test_zone
endif

if IS_TRU64
check_PROGRAMS += test_persist_state
else
if IS_FBSD61
check_PROGRAMS += test_persist_state
else
check_PROGRAMS += test_persist_state test_zone
endif
endif

TEST_DEPENDENCIES = libtest.c libtest_memreader.c libtest_memreader.h testutils.c testutils.h

test_msgparse_SOURCES = test_msgparse.c $(TEST_DEPENDENCIES)
test_template_SOURCES = test_template.c $(TEST_DEPENDENCIES)
test_template_LDADD = $(LDADD) $(top_builddir)/modules/basicfuncs/libbasicfuncs.la $(top_builddir)/modules/convertfuncs/libconvertfuncs.la $(top_builddir)/modules/tfjson/libtfjson.la
test_template_speed_SOURCES = test_template_speed.c $(TEST_DEPENDENCIES)
test_zone_SOURCES = test_zone.c  $(TEST_DEPENDENCIES)
test_dnscache_SOURCES = test_dnscache.c  $(TEST_DEPENDENCIES)
test_serialize_SOURCES = test_serialize.c  $(TEST_DEPENDENCIES)
test_findeom_SOURCES = test_findeom.c  $(TEST_DEPENDENCIES)
test_findcrlf_SOURCES = test_findcrlf.c  $(TEST_DEPENDENCIES)
test_csvparser_SOURCES = test_csvparser.c  $(TEST_DEPENDENCIES)
test_csvparser_LDADD = $(LDADD) $(top_builddir)/modules/csvparser/libcsvparser.la
test_clone_logmsg_SOURCES = test_clone_logmsg.c $(TEST_DEPENDENCIES)
test_matcher_SOURCES = test_matcher.c $(TEST_DEPENDENCIES)
test_filters_SOURCES = test_filters.c $(TEST_DEPENDENCIES)
test_logqueue_SOURCES = test_logqueue.c
test_msgsdata_SOURCES = test_msgsdata.c $(TEST_DEPENDENCIES) msg_parse_lib.h
test_tags_SOURCES = test_tags.c
test_nvtable_SOURCES = test_nvtable.c
test_logwriter_SOURCES = test_logwriter.c
test_resolve_pwgr_SOURCES = test_resolve_pwgr.c
test_persist_state_SOURCES = test_persist_state.c
test_value_pairs_SOURCES = test_value_pairs.c
test_filereader_SOURCES = test_filereader.c $(TEST_DEPENDENCIES)
test_multiline_SOURCES = test_multiline.c $(TEST_DEPENDENCIES)

test_msgserialize_benchmark_SOURCES = test_msgserialize_benchmark.c
test_msgserialize_tags_SOURCES = test_msgserialize_tags.c
test_msgformat_upgrade_SOURCES = test_msgformat_upgrade.c

test_thread_wakeup_SOURCES = test_thread_wakeup.c
test_rcptid_SOURCES = test_rcptid.c

TESTS = $(check_PROGRAMS)

