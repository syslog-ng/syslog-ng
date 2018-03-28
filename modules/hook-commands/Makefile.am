module_LTLIBRARIES += modules/hook-commands/libhook-commands.la

modules_hook_commands_libhook_commands_la_SOURCES = \
  modules/hook-commands/hook-commands.c \
  modules/hook-commands/hook-commands.h \
  modules/hook-commands/hook-commands-grammar.y \
  modules/hook-commands/hook-commands-parser.c \
  modules/hook-commands/hook-commands-parser.h \
  modules/hook-commands/hook-commands-plugin.c

BUILT_SOURCES += \
  modules/hook-commands/hook-commands-grammar.y \
  modules/hook-commands/hook-commands-grammar.c \
  modules/hook-commands/hook-commands-grammar.h

EXTRA_DIST += modules/hook-commands/hook-commands-grammar.ym

modules_hook_commands_libhook_commands_la_CPPFLAGS = $(AM_CPPFLAGS) -I$(top_srcdir)/modules/hook-commands -I$(top_builddir)/modules/hook-commands
modules_hook_commands_libhook_commands_la_LIBADD = $(MODULE_DEPS_LIBS)
modules_hook_commands_libhook_commands_la_LDFLAGS = $(MODULE_LDFLAGS)
modules_hook_commands_libhook_commands_la_DEPENDENCIES = $(MODULE_DEPS_LIBS)

modules/hook-commands modules/hook-commands/ mod-hook-commands: modules/hook-commands/libhook-commands.la

#include modules/hook-commands/tests/Makefile.am

.PHONY: modules/hook-commands/ mod-hook-commands
