pkginclude_HEADERS			+= \
		persist-tool/add.h \
		persist-tool/dump.h \
		persist-tool/generate.h \
		persist-tool/persist-tool.h \
		persist-tool/state.h

bin_PROGRAMS			+= persist-tool/persist-tool
persist_tool_persist_tool_CPPFLAGS	= \
	-I$(top_srcdir)/lib \
	-I$(top_srcdir)/lib/jsonc \
	$(JSON_CFLAGS)

persist_tool_persist_tool_SOURCES	=	\
	persist-tool/add.c	\
	persist-tool/dump.c \
	persist-tool/generate.c \
	persist-tool/persist-tool.c \
	persist-tool/state.c

persist_tool_persist_tool_LDADD	= \
  $(TEST_LDADD) \
	@GLIB_LIBS@ \
	@BASE_LIBS@ \
	@JSON_LIBS@

EXTRA_DIST += persist-tool/CMakeLists.txt
