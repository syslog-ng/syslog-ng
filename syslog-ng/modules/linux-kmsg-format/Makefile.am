module_LTLIBRARIES 						+=	\
	modules/linux-kmsg-format/liblinux-kmsg-format.la

EXTRA_DIST += modules/linux-kmsg-format/CMakeLists.txt

modules_linux_kmsg_format_liblinux_kmsg_format_la_SOURCES	=	\
	modules/linux-kmsg-format/linux-kmsg-format.c			\
	modules/linux-kmsg-format/linux-kmsg-format.h			\
	modules/linux-kmsg-format/linux-kmsg-format-plugin.c

modules_linux_kmsg_format_liblinux_kmsg_format_la_CPPFLAGS	=	\
	$(AM_CPPFLAGS)							\
	-I${top_srcdir}/modules/linux-kmsg-format			\
	-I${top_builddir}/modules/linux-kmsg-format
modules_linux_kmsg_format_liblinux_kmsg_format_la_LIBADD	=	\
	$(MODULE_DEPS_LIBS)
modules_linux_kmsg_format_liblinux_kmsg_format_la_LDFLAGS	=	\
	$(MODULE_LDFLAGS)
modules_linux_kmsg_format_liblinux_kmsg_format_la_DEPENDENCIES	=	\
	$(MODULE_DEPS_LIBS)

modules/linux-kmsg-format modules/linux-kmsg-format/ mod-linux-kmsg:	\
	modules/linux-kmsg-format/liblinux-kmsg-format.la
.PHONY: modules/linux-kmsg-format/ mod-linux-kmsg

include modules/linux-kmsg-format/tests/Makefile.am
