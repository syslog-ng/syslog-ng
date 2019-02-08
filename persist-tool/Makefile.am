bin_PROGRAMS			+= persist-tool/persist-tool

persist_tool_persist_tool_CPPFLAGS	= \
	-I$(top_srcdir)/lib

persist_tool_persist_tool_SOURCES	=	\
	persist-tool/add.c	\
	persist-tool/add.h \
	persist-tool/dump.c \
	persist-tool/dump.h \
	persist-tool/generate.c \
	persist-tool/generate.h \
	persist-tool/persist-tool.c \
	persist-tool/persist-tool.h 

persist_tool_persist_tool_LDADD	= \
  $(TEST_LDADD) \
	@GLIB_LIBS@ \
	@BASE_LIBS@

EXTRA_DIST += persist-tool/CMakeLists.txt
