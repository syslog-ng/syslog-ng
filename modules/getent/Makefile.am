module_LTLIBRARIES			+= \
  modules/getent/libtfgetent.la

modules_getent_libtfgetent_la_SOURCES	= \
  modules/getent/tfgetent.c

EXTRA_DIST				+=\
  modules/getent/getent-protocols.c	  \
  modules/getent/getent-services.c	  \
  modules/getent/getent-group.c	  \
  modules/getent/getent-passwd.c	\
  modules/getent/CMakeLists.txt

modules_getent_libtfgetent_la_CPPFLAGS	= \
  $(AM_CPPFLAGS)			  \
  -I$(top_srcdir)/modules/getent
modules_getent_libtfgetent_la_LIBADD	= \
  $(MODULE_DEPS_LIBS)
modules_getent_libtfgetent_la_LDFLAGS	= \
  $(MODULE_LDFLAGS)
modules_getent_libtfgetent_la_DEPENDENCIES = \
  $(MODULE_DEPS_LIBS)

modules/getent mod-getent: modules/getent/libtfgetent.la

.PHONY: modules/getent mod-getent
