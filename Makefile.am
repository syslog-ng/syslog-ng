SUBDIRS = lib libtest modules syslog-ng syslog-ng-ctl scripts tests doc contrib scl debian tgz2build build winbuild

# tests from pe
SUBDIRS += pe-tests

ACLOCAL_AMFLAGS = -I m4 --install

EXTRA_DIST = VERSION \
	syslog-ng.spec syslog-ng.spec.in dist.conf dist.conf.in ChangeLog syslog-ng.pc.in balabit-vs-build makefile.msc

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = syslog-ng.pc

.PHONY: ChangeLog

check_%:
	@target=$@ ; \
        if test $$target = 'check_unit' ; then \
                $(MAKE) SUBDIRS=tests/unit check ; \
        elif test $$target = 'check_functional' ; then \
                $(MAKE) SUBDIRS=pe-tests/functional check ; \
        elif test $${target:0:19} = 'check_unit_modules_' ; then \
                $(MAKE) SUBDIRS=modules/`echo "$@" | sed -r 's/^check_unit_modules_//g'`/tests check ; \
        elif test $${target:0:11} = 'check_unit_' ; then \
                $(MAKE) SUBDIRS=tests/unit check_PROGRAMS=`echo "$@" | sed -r 's/^check_unit_//g'` check ; \
        elif test $${target:0:17} = 'check_functional_' ; then \
                $(MAKE) SUBDIRS=pe-tests/functional testcase=`echo "$@" | sed -r 's/^check_functional_//g'` check ; \
        else \
                echo "Unknown type of test: '`echo $@ | sed -r 's/^check_([^_]+)_.*/\\1/g'`'" ; \
        fi

ChangeLog:
	(cd $(top_srcdir); git log) > $@
	
