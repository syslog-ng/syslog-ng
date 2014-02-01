if ENABLE_PERSIST_TOOL
bin_PROGRAMS           += persist-tool/persist-tool

noinst_LTLIBRARIES += persist-tool/libpersist-tool.la

persist_tool_libpersist_tool_la_SOURCES =   \
	persist-tool/json-presented-persistable-state.h \
	persist-tool/json-presented-persistable-state.c \
	persist-tool/dump.c	\
	persist-tool/dump.h 	\
	persist-tool/add.c 	\
	persist-tool/add.h 	\
	persist-tool/generate-common.c 	\
	persist-tool/generate-common.h 	\
	persist-tool/generate.c 	\
	persist-tool/generate.h

persist_tool_libpersist_tool_la_CFLAGS = $(JSON_CFLAGS)

persist_tool_libpersist_tool_la_LIBADD = lib/libsyslog-ng.la lib/libsyslog-ng-crypto.la $(TOOL_DEPS_LIBS)

persist_tool_persist_tool_SOURCES =   \
	persist-tool/persist-tool.c	\
	persist-tool/persist-tool.h

persist_tool_persist_tool_LDADD = persist-tool/libpersist-tool.la $(JSON_LIBS)

include persist-tool/tests/Makefile.am

endif
