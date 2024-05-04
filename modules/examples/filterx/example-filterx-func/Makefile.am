# noinst: Built as part of a larger libexamples module as static library
noinst_LTLIBRARIES      += modules/examples/filterx/example-filterx-func/libexample-filterx-func.la
modules_examples_filterx_example_filterx_func_libexample_filterx_func_la_SOURCES = \
  modules/examples/filterx/example-filterx-func/example-filterx-func-plugin.h   \
  modules/examples/filterx/example-filterx-func/example-filterx-func-plugin.c

EXTRA_DIST        +=      \
  modules/examples/filterx/example-filterx-func/CMakeLists.txt

modules_examples_filterx_example_filterx_func_libexample_filterx_func_la_CPPFLAGS  =     \
  $(AM_CPPFLAGS)            \
  -I$(top_srcdir)/modules/examples/filterx/example-filterx-func        \
  -I$(top_builddir)/modules/examples/filterx/example-filterx-func
modules_examples_filterx_example_filterx_func_libexample_filterx_func_la_LIBADD  = $(MODULE_DEPS_LIBS)
modules_examples_filterx_example_filterx_func_libexample_filterx_func_la_LDFLAGS = $(MODULE_LDFLAGS)
EXTRA_modules_examples_filterx_example_filterx_func_libexample_filterx_func_la_DEPENDENCIES= $(MODULE_DEPS_LIBS)
