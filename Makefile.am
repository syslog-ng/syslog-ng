SUBDIRS			=
AM_MAKEFLAGS		= --no-print-directory
AM_YFLAGS		= -Wno-yacc -Wno-other

AM_TESTS_ENVIRONMENT	= top_srcdir="$(top_srcdir)"

ACLOCAL_AMFLAGS		= -I m4 --install

EXTRA_DIST		= ${BUILT_SOURCES} VERSION autogen.sh \
			  syslog-ng.spec syslog-ng.spec.in \
			  dist.conf dist.conf.in syslog-ng.pc.in

pkgconfigdir		= $(libdir)/pkgconfig
pkgconfig_DATA		= syslog-ng.pc

AM_CPPFLAGS		= -I$(top_srcdir)/lib -I$(top_srcdir)/modules -I$(top_builddir)/lib -I$(top_builddir)/modules
TEST_CFLAGS		= -I$(top_srcdir)/libtest
TEST_LDADD		= $(top_builddir)/lib/libsyslog-ng.la \
			  $(top_builddir)/libtest/libsyslog-ng-test.a \
			  $(TOOL_DEPS_LIBS)
test_ldflags		= -no-install

PREOPEN_SYSLOGFORMAT	= -dlpreopen ${top_builddir}/modules/syslogformat/libsyslogformat.la
PREOPEN_BASICFUNCS	= -dlpreopen ${top_builddir}/modules/basicfuncs/libbasicfuncs.la
PREOPEN_CORE		= $(PREOPEN_SYSLOGFORMAT) $(PREOPEN_BASICFUNCS)

syslog_ng_tools		= ${top_srcdir}/lib

export top_srcdir

CLEAN_SUBDIRS		=

clean-local:
	${AM_v_at}for dir in ${CLEAN_SUBDIRS}; do \
		[ -f $${dir}/Makefile ] && ${MAKE} -C $${dir} clean || true; \
	done

local-check: subdir_tests=$(foreach ts,${${check_subdir}_test_subdirs},${${ts}_tests_TESTS})
local-check: current_tests=$(foreach subdir,${check_subdir} ${check_subdir}_tests,${${subdir}_TESTS})
local-check:
	${AM_v_at}${MAKE} check check_PROGRAMS="${current_tests} ${subdir_tests}" \
				TESTS="${current_tests} ${subdir_tests}"

${check_PROGRAMS}: LDFLAGS+="${test_ldflags}"

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
UNINSTALL_HOOKS		=

AUTOMAKE_OPTIONS	= subdir-objects

${top_srcdir}/ChangeLog:
	(cd $(top_srcdir); git log) > $@

install-exec-hook: ${INSTALL_EXEC_HOOKS}
uninstall-hook: ${UNINSTALL_HOOKS}

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
