SUBDIRS			=
AM_MAKEFLAGS		= --no-print-directory

ACLOCAL_AMFLAGS		= -I m4 --install

EXTRA_DIST		= ${BUILT_SOURCES} VERSION \
			  syslog-ng.spec syslog-ng.spec.in \
			  dist.conf dist.conf.in syslog-ng.pc.in \
			  ${top_srcdir}/ChangeLog

pkgconfigdir		= $(libdir)/pkgconfig
pkgconfig_DATA		= syslog-ng.pc

AM_CPPFLAGS		= -I$(top_srcdir)/lib -I$(top_srcdir)/modules -I$(top_builddir)/lib -I$(top_builddir)/modules
TEST_CFLAGS		= -I$(top_srcdir)/libtest
TEST_LDADD		= $(top_builddir)/lib/libsyslog-ng.la \
			  $(top_builddir)/libtest/libsyslog-ng-test.a \
			  $(TOOL_DEPS_LIBS)

export top_srcdir

CLEAN_SUBDIRS		=

clean-local:
	${AM_v_at}for dir in ${CLEAN_SUBDIRS}; do \
		[ -f $${dir}/Makefile ] && ${MAKE} -C $${dir} clean || true; \
	done

local-check:
	${AM_v_at}${MAKE} check check_PROGRAMS="${${check_subdir}_TESTS}" TESTS="${${check_subdir}_TESTS}"

noinst_LIBRARIES	=
noinst_DATA		=
noinst_LTLIBRARIES	=
lib_LTLIBRARIES		=
pkginclude_HEADERS	=
module_LTLIBRARIES	=
BUILT_SOURCES		=
CLEANFILES 		= $(BUILT_SOURCES)
check_PROGRAMS		=
TESTS			= $(check_PROGRAMS)
bin_SCRIPTS		=
bin_PROGRAMS		=
sbin_PROGRAMS		=
libexec_PROGRAMS	=
man_MANS		=

INSTALL_EXEC_HOOKS	=

AUTOMAKE_OPTIONS	= subdir-objects

${top_srcdir}/ChangeLog:
	(cd $(top_srcdir); git log) > $@

install-exec-hook: ${INSTALL_EXEC_HOOKS}

help:
	@echo "Welcome to the syslog-ng build system!"
	@echo
	@echo "All of the standard targets are available:"
	@echo "  all, check, install, dist, distcheck, and clean"
	@echo
	@echo "Apart from these, the build system supports various other,"
	@echo "special targets:"
	@echo
	@echo " modules  : Builds all syslog-ng modules."
	@echo " syslog-ng: Build syslog-ng itself, only."
	@echo
	@echo "One can also build individual modules (and their dependencies),"
	@echo "using any of the following shortcuts:"
	@echo
	@echo "" ${SYSLOG_NG_MODULES} | sed -e 's#\(.\{,72\}\) #\1\n #g'

.PHONY: help

include Mk/lex-rules.am
include libtest/Makefile.am
include lib/Makefile.am
include modules/Makefile.am
include syslog-ng/Makefile.am
include scripts/Makefile.am
include tests/Makefile.am
include doc/Makefile.am
include contrib/Makefile.am
include scl/Makefile.am
include debian/Makefile.am
include tgz2build/Makefile.am
include Mk/Makefile.am
