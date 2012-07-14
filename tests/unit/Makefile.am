
# we need the dotdot version of the include path to find generated headers
AM_CFLAGS = -I$(top_srcdir)/lib -I$(top_srcdir)/libtest -I../../lib
AM_LDFLAGS = -dlpreopen $(top_builddir)/modules/syslogformat/libsyslogformat.la
LIBS = @TOOL_DEPS_LIBS@
LDADD = $(top_builddir)/lib/libsyslog-ng.la $(top_builddir)/libtest/libsyslog-ng-test.a

check_PROGRAMS =			\
	test_resolve_pwgr		\
	test_nvtable			\
	test_msgsdata			\
	test_logqueue			\
	test_matcher			\
	test_clone_logmsg 		\
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
	test_logproto			\
	test_serialize			\
	test_zone			\
	test_persist_state		\
	test_value_pairs

test_msgparse_SOURCES = test_msgparse.c
test_template_SOURCES = test_template.c
test_template_LDADD = $(LDADD) -dlpreopen $(top_builddir)/modules/basicfuncs/libbasicfuncs.la
test_template_speed_SOURCES = test_template_speed.c
test_zone_SOURCES = test_zone.c
test_dnscache_SOURCES = test_dnscache.c
test_serialize_SOURCES = test_serialize.c
test_findeom_SOURCES = test_findeom.c
test_findcrlf_SOURCES = test_findcrlf.c
test_clone_logmsg_SOURCES = test_clone_logmsg.c
test_matcher_SOURCES = test_matcher.c
test_filters_SOURCES = test_filters.c
test_logqueue_SOURCES = test_logqueue.c
test_msgsdata_SOURCES = test_msgsdata.c
test_tags_SOURCES = test_tags.c
test_nvtable_SOURCES = test_nvtable.c
test_logwriter_SOURCES = test_logwriter.c
test_resolve_pwgr_SOURCES = test_resolve_pwgr.c
test_persist_state_SOURCES = test_persist_state.c
test_value_pairs_SOURCES = test_value_pairs.c
test_logproto_SOURCES = test_logproto.c

TESTS = $(check_PROGRAMS)

