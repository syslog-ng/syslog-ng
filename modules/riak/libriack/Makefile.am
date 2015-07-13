# -- Global settings --
AM_MAKEFLAGS			= --no-print-directory
AM_CFLAGS			= -I${top_srcdir}/lib -I${top_builddir}/lib
ACLOCAL_AMFLAGS			= -Im4

LDADD				= lib/libriack.la

pkgincludedir			= ${includedir}/riack
protodir			= ${pkgincludedir}/proto

VERSION				= @VERSION@${SNAPSHOT_VERSION}

# -- Libraries --
LRC_CURRENT			= 0
LRC_REVISION			= 0
LRC_AGE				= 0

lib_LTLIBRARIES			= lib/libriack.la

lib_libriack_CFLAGS		= ${AM_CFLAGS} ${PROTOBUF_C_CFLAGS}
lib_libriack_la_LDFLAGS		= \
	${PROTOBUF_C_LIBS}	  \
	${GNUTLS_LIBS}		  \
	-version-info ${LRC_CURRENT}:${LRC_REVISION}:${LRC_AGE}
riak_proto_files		= \
	lib/riack/proto/riak.pb-c.c  \
	lib/riack/proto/riak.pb-c.h
riak_kv_proto_files		= \
	lib/riack/proto/riak_kv.pb-c.c  \
	lib/riack/proto/riak_kv.pb-c.h
riak_dt_proto_files		= \
	lib/riack/proto/riak_dt.pb-c.c  \
	lib/riack/proto/riak_dt.pb-c.h
nodist_proto_HEADERS		= \
	lib/riack/proto/riak.pb-c.h \
	lib/riack/proto/riak_kv.pb-c.h  \
	lib/riack/proto/riak_dt.pb-c.h
pkginclude_HEADERS		= \
	lib/riack/client.h	  \
	lib/riack/content.h	  \
	lib/riack/riack.h   \
	lib/riack/putreq.h  \
	lib/riack/message.h \
	lib/riack/dtupdatereq.h \
	lib/riack/dtop.h \
	lib/riack/setop.h 
lib_libriack_la_SOURCES		= \
	lib/riack/client.c	  \
	lib/riack/content.c    \
	lib/riack/putreq.c  \
	lib/riack/dtupdatereq.c  \
	lib/riack/dtop.c  \
	lib/riack/setop.c  
$(am_lib_libriack_la_OBJECTS): ${riak_proto_files} ${riak_kv_proto_files} ${riak_dt_proto_files}
noinst_HEADERS			= \
	lib/riack/_private.h

nodist_lib_libriack_la_SOURCES	= \
	${riak_proto_files} ${riak_kv_proto_files} ${riak_dt_proto_files}

CLEANFILES			= ${riak_proto_files} ${riak_kv_proto_files} ${riak_dt_proto_files}

${riak_proto_files}: ${top_srcdir}/ext/riak_pb/src/riak.proto
	${AM_V_at} ${mkinstalldirs} ${top_builddir}/lib/riack/proto
	${AM_V_GEN} protoc-c $^ -I${top_srcdir}/ext/riak_pb/src --c_out=${top_builddir}/lib/riack/proto

${riak_kv_proto_files}: ${top_srcdir}/ext/riak_pb/src/riak_kv.proto
	${AM_V_at} ${mkinstalldirs} ${top_builddir}/lib/riack/proto
	${AM_V_GEN} protoc-c $^ -I${top_srcdir}/ext/riak_pb/src --c_out=${top_builddir}/lib/riack/proto
	
${riak_dt_proto_files}: ${top_srcdir}/ext/riak_pb/src/riak_dt.proto
	${AM_V_at} ${mkinstalldirs} ${top_builddir}/lib/riack/proto
	${AM_V_GEN} protoc-c $^ -I${top_srcdir}/ext/riak_pb/src --c_out=${top_builddir}/lib/riack/proto

if HAVE_VERSIONING
lib_libriack_la_LDFLAGS		+= \
	-Wl,--version-script,$(top_srcdir)/lib/riack/libriack.ver
lib_libriack_la_DEPENDENCIES	= ${top_srcdir}/lib/riack/libriack.ver
endif

pkgconfigdir			= $(libdir)/pkgconfig
pkgconfig_DATA			= lib/riack/riack.pc

# -- Testcases --
if HAVE_CHECK
UNIT_TESTS			= tests/check_libriack
TESTS				= ${UNIT_TESTS}

tests/check_%: CFLAGS += ${CHECK_CFLAGS}
tests/check_%: LDADD += ${CHECK_LIBS} -ldl
tests/check_%: LDFLAGS += -no-install

check_PROGRAMS			= ${TESTS}
endif

check_libriack_srcs		= \
	tests/tests.h		  \
	tests/check_library.c	  \
	tests/check_client.c	  \
	tests/check_content.c	  \
	tests/check_libriack.c    \
	tests/check_putreq.c    \
	tests/check_dtupdatereq.c   \
	tests/check_setop.c \
	tests/check_dtop.c

# -- Extra files to distribute --
EXTRA_DIST			= README.md NEWS.md GPL LGPL \
				  ext/ \
				  lib/riack/libriack.ver \
				  lib/riack/riack.pc \
				  ${check_libriack_srcs}

# -- Custom targets --
coverage: coverage.info.html

coverage.info.html: coverage.info
	$(AM_V_GEN)genhtml --quiet $^ --output-directory $@

coverage.info: check
	$(AM_V_GEN)lcov --quiet --capture --directory ${top_builddir}/lib --output $@ -b ${top_builddir} && \
		   lcov --quiet --remove $@ '*/lib/riack/proto/*' -o $@

.PHONY: coverage
CLEANFILES			+= coverage.info

clean-local:
	rm -rf coverage.info.html
	find ${top_builddir} -name '*.gcda' -or -name '*.gcno' | xargs rm -f

docs/html: Doxyfile $(pkginclude_HEADERS)
	$(AM_V_GEN)doxygen

doxygen: docs/html

.PHONY: doxygen
