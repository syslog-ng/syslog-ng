SUBDIRS = filters-in-list

# we need the dotdot version of the include path to find generated headers
AM_CFLAGS = -I$(top_srcdir)/lib -I$(top_srcdir)/libtest -I../../lib -I$(top_srcdir)/modules/basic-proto -I$(top_srcdir)/modules
AM_LDFLAGS = -dlpreopen $(top_builddir)/modules/syslogformat/libsyslogformat.la -dlpreopen $(top_builddir)/modules/basic-proto/libbasic-proto.la
LIBS = @TOOL_DEPS_LIBS@
LDADD = $(top_builddir)/lib/libsyslog-ng.la $(top_builddir)/libtest/libsyslog-ng-test.a @TOOL_DEPS_LIBS@

export top_srcdir

check_PROGRAMS =			\
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
	test_filters_in_list			\
	test_dnscache			\
	test_findeom			\
	test_findcrlf			\
	test_tags			\
	test_logwriter			\
	test_state \
	test_buffered_proto_state \
	test_serialize			\
	test_value_pairs		\
	test_value_pairs_walk   \
	test_filereader			\
	test_multiline			\
	test_msgserialize_benchmark	\
	test_msgserialize_tags		\
	test_msgformat_upgrade \
	test_hostname	\
	test_cache \
	test_data_to_hex \
	test_control_cmds \
	test_control_connection \
	test_messages \
	test_rcptid \
	test_strformat \
	test_msgserialize \
	test_lexer \
	test_replace_string \
	test_csv_parser_flags \
	test_persist_state_threaded \
	test_resolve_sockaddr \
	test_strtok_r \
	test_template_compile \
	test_logproto	\
	test_rewrite  \
	test_resolve_path  \
	test_ringbuffer \
	test_mainloop

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

if !IS_WINDOWS
check_PROGRAMS +=	test_thread_wakeup \
	                test_resolve_pwgr
endif

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
test_filters_in_list_SOURCES = test_filters_in_list.c
test_logqueue_SOURCES = test_logqueue.c
test_msgsdata_SOURCES = test_msgsdata.c
test_tags_SOURCES = test_tags.c
test_nvtable_SOURCES = test_nvtable.c
test_logwriter_SOURCES = test_logwriter.c
test_resolve_pwgr_SOURCES = test_resolve_pwgr.c
test_persist_state_SOURCES = test_persist_state.c
test_value_pairs_SOURCES = test_value_pairs.c
test_value_pairs_walk_SOURCES = test_value_pairs_walk.c
test_filereader_SOURCES = test_filereader.c $(top_srcdir)/libtest/libtest_memreader.c $(top_srcdir)/libtest/libtest_memreader.h
test_multiline_SOURCES = test_multiline.c $(top_srcdir)/libtest/libtest_memreader.c $(top_srcdir)/libtest/libtest_memreader.h
test_csv_parser_flags_SOURCES = test_csv_parser_flags.c
test_csv_parser_flags_LDADD = $(LDADD) $(top_builddir)/modules/csvparser/libcsvparser.la
test_msgserialize_benchmark_SOURCES = test_msgserialize_benchmark.c
test_msgserialize_tags_SOURCES = test_msgserialize_tags.c
test_msgformat_upgrade_SOURCES = test_msgformat_upgrade.c

test_thread_wakeup_SOURCES = test_thread_wakeup.c
test_rcptid_SOURCES = test_rcptid.c

test_hostname_SOURCES = test_hostname.c
test_data_to_hex_SOURCES = test_data_to_hex.c
test_persist_state_threaded_SOURCES = test_persist_state_threaded.c
test_resolve_sockaddr_SOURCES = test_resolve_sockaddr.c
test_strtok_r_SOURCES = test_strtok_r.c $(top_srcdir)/lib/compat.c
test_strtok_r_CFLAGS = $(AM_CFLAGS) -DUSE_MYSTRTOK_R
test_strtok_r_LIBS = -lm
test_strtok_r_LDADD =

test_cache_SOURCES = test_cache.c
test_control_cmds_SOURCES = test_control_cmds.c
test_control_connection_SOURCES = test_control_connection.c
test_state_SOURCES = test_state.c
test_buffered_proto_state_SOURCES = test_buffered_proto_state.c
test_messages_SOURCES = test_messages.c

test_strformat_SOURCES = test_strformat.c

test_msgserialize_SOURCES = test_msgserialize.c

test_lexer_SOURCES = test_lexer.c

test_replace_string_SOURCES = test_replace_string.c

test_template_compile_SOURCES = test_template_compile.c

test_logproto_SOURCES = test_logproto.c
test_rewrite_SOURCES = test_rewrite.c
test_ringbuffer_SOURCES = test_ringbuffer.c

test_resolve_path_SOURCES = test_resolve_path.c
test_resolve_path_CFLAGS = $(AM_CFLAGS) -I$(top_srcdir)/modules/affile
test_resolve_path_LDADD = $(top_builddir)/modules/affile/libaffile.la

test_mainloop_SOURCES = test_mainloop.c

TESTS = $(check_PROGRAMS)
