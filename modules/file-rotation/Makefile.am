module_LTLIBRARIES      += modules/file-rotation/libfile-rotation.la
modules_file_rotation_libfile_rotation_la_SOURCES = \
  modules/file-rotation/file-rotation-grammar.y       \
  modules/file-rotation/file-rotation-parser.c        \
  modules/file-rotation/file-rotation-parser.h        \
  modules/file-rotation/file-rotation-plugin.c        \
  modules/file-rotation/file-rotation.c \
  modules/file-rotation/file-rotation.h

BUILT_SOURCES       +=      \
  modules/file-rotation/file-rotation-grammar.y       \
  modules/file-rotation/file-rotation-grammar.c       \
  modules/file-rotation/file-rotation-grammar.h

EXTRA_DIST        +=      \
  modules/file-rotation/file-rotation-grammar.ym \
  modules/file-rotation/CMakeLists.txt

modules_file_rotation_libfile_rotation_la_CPPFLAGS  =     \
  $(AM_CPPFLAGS)            \
  -I$(top_srcdir)/modules/file-rotation        \
  -I$(top_builddir)/modules/file-rotation
modules_file_rotation_libfile_rotation_la_LIBADD  = $(MODULE_DEPS_LIBS)
modules_file_rotation_libfile_rotation_la_LDFLAGS = $(MODULE_LDFLAGS)
modules_file_rotation_libfile_rotation_la_DEPENDENCIES= $(MODULE_DEPS_LIBS)

modules/file-rotation modules/file-rotation/ mod-file-rotation: modules/file-rotation/libfile-rotation.la
.PHONY: modules/file-rotation/ mod-file-rotation

include modules/file-rotation/tests/Makefile.am
