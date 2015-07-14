# -- Global settings --

noinst_LTLIBRARIES				+= \
	modules/riak/libriack/lib/libriack.la

riak_proto_files		= \
	modules/riak/libriack/lib/riack/proto/riak.pb-c.c  \
	modules/riak/libriack/lib/riack/proto/riak.pb-c.h
riak_kv_proto_files		= \
	modules/riak/libriack/lib/riack/proto/riak_kv.pb-c.c  \
	modules/riak/libriack/lib/riack/proto/riak_kv.pb-c.h
riak_dt_proto_files		= \
	modules/riak/libriack/lib/riack/proto/riak_dt.pb-c.c  \
	modules/riak/libriack/lib/riack/proto/riak_dt.pb-c.h
noinst_HEADERS		= \
	modules/riak/libriack/lib/riack/proto/riak.pb-c.h \
	modules/riak/libriack/lib/riack/proto/riak_kv.pb-c.h  \
	modules/riak/libriack/lib/riack/proto/riak_dt.pb-c.h \
	modules/riak/libriack/lib/riack/_private.h

modules_riak_libriack_lib_libriack_la_SOURCES	= \
	modules/riak/libriack/lib/riack/client.h	  \
	modules/riak/libriack/lib/riack/content.h	  \
	modules/riak/libriack/lib/riack/riack.h   \
	modules/riak/libriack/lib/riack/putreq.h  \
	modules/riak/libriack/lib/riack/message.h \
	modules/riak/libriack/lib/riack/dtupdatereq.h \
	modules/riak/libriack/lib/riack/dtop.h \
	modules/riak/libriack/lib/riack/setop.h \
	modules/riak/libriack/lib/riack/client.c	  \
	modules/riak/libriack/lib/riack/content.c    \
	modules/riak/libriack/lib/riack/putreq.c  \
	modules/riak/libriack/lib/riack/dtupdatereq.c  \
	modules/riak/libriack/lib/riack/dtop.c  \
	modules/riak/libriack/lib/riack/setop.c \
	${riak_proto_files} ${riak_kv_proto_files} ${riak_dt_proto_files}

modules_riak_libriack_lib_libriack_la_CFLAGS	 =  \
	-I$(top_srcdir)/modules/riak/libriack/lib   \
	-I$(top_builddir)/modules/riak/libriack/lib \
	${PROTOBUF_C_CFLAGS}
modules_riak_libriack_lib_libriack_la_LDFLAGS    = \
	${PROTOBUF_C_LIBS}

$(am_modules_riak_libriack_lib_libriack_la_OBJECTS): ${riak_proto_files} ${riak_kv_proto_files} ${riak_dt_proto_files}

CLEANFILES			+= ${riak_proto_files} ${riak_kv_proto_files} ${riak_dt_proto_files}

proto_dir = modules/riak/libriack/ext/riak_pb/src
proto_dest_dir = modules/riak/libriack/lib/riack/proto

${riak_proto_files}: ${top_srcdir}/${proto_dir}/riak.proto
	${AM_V_at} ${mkinstalldirs} ${top_builddir}/${proto_dest_dir}
	${AM_V_GEN} protoc-c $^ -I${top_srcdir}/${proto_dir} --c_out=${top_builddir}/${proto_dest_dir}

${riak_kv_proto_files}: ${top_srcdir}/${proto_dir}/riak_kv.proto
	${AM_V_at} ${mkinstalldirs} ${top_builddir}/${proto_dest_dir}
	${AM_V_GEN} protoc-c $^ -I${top_srcdir}/${proto_dir} --c_out=${top_builddir}/${proto_dest_dir}

${riak_dt_proto_files}: ${top_srcdir}/${proto_dir}/riak_dt.proto
	${AM_V_at} ${mkinstalldirs} ${top_builddir}/${proto_dest_dir}
	${AM_V_GEN} protoc-c $^ -I${top_srcdir}/${proto_dir} --c_out=${top_builddir}/${proto_dest_dir}

# -- Testcases --
if HAVE_CHECK
modules_riak_libriack_tests_TESTS = modules/riak/libriack/tests/check_libriack

modules/riak/libriack/tests/check_%: CFLAGS += ${CHECK_CFLAGS} \
	${modules_riak_libriack_lib_libriack_la_CFLAGS}
modules/riak/libriack/tests/check_%: LDADD += ${CHECK_LIBS} -ldl \
	modules/riak/libriack/lib/libriack.la ${PROTOBUF_C_LIBS}
modules/riak/libriack/tests/check_%: LDFLAGS += -no-install

check_PROGRAMS += ${modules_riak_libriack_tests_TESTS}
endif

check_libriack_srcs		= \
	modules/riak/libriack/tests/tests.h		  \
	modules/riak/libriack/tests/check_client.c	  \
	modules/riak/libriack/tests/check_content.c	  \
	modules/riak/libriack/tests/check_libriack.c    \
	modules/riak/libriack/tests/check_putreq.c    \
	modules/riak/libriack/tests/check_dtupdatereq.c   \
	modules/riak/libriack/tests/check_setop.c \
	modules/riak/libriack/tests/check_dtop.c

# -- Extra files to distribute --
EXTRA_DIST			+= modules/riak/libriack/ext/ \
				  ${check_libriack_srcs}
