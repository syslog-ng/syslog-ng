export top_srcdir
moduledir = @moduledir@
AM_CPPFLAGS = -I$(top_srcdir)/lib -I../lib

bin_PROGRAMS = persist-tool

persist_tool_SOURCES = persist-tool.c persist-tool.h dump.c dump.h add.c add.h generate.c generate.h generate-agent.c generate-agent.h $(top_srcdir)/lib/persist-state.c

persist_tool_LDADD =  $(SYSLOG_NG_LIBS) -ljson
