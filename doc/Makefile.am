xsd_DATA	= \
		doc/xsd/patterndb-1.xsd \
		doc/xsd/patterndb-2.xsd \
		doc/xsd/patterndb-3.xsd \
		doc/xsd/patterndb-4.xsd \
		doc/xsd/patterndb-5.xsd \
		doc/xsd/patterndb-6.xsd


EXTRA_DIST	+=  \
		doc/man/update-manpages.sh \
		doc/security/bof-2002-09-27.txt \
		doc/security/dos-2000-11-22.txt \
		doc/xsd/examples/patterndb-1.xml \
		$(xsd_DATA)

manpages	= \
		doc/man/dqtool.1 \
		doc/man/loggen.1 \
		doc/man/pdbtool.1 \
		doc/man/persist-tool.1 \
		doc/man/syslog-ng-debun.1 \
		doc/man/syslog-ng-ctl.1 \
		doc/man/syslog-ng.8 \
		doc/man/syslog-ng.conf.5 \
		doc/man/slogencrypt.1 \
		doc/man/slogkey.1 \
		doc/man/slogverify.1 \
		doc/man/secure-logging.7

# "dist" will always need the manpages, so we either need the documentation
# tools, or the make dist needs to run from a release tarball, which ships
# the manual pages.

EXTRA_DIST	+= $(manpages)
man_MANS        += $(manpages)

sysconfdir_e = $(shell echo "${sysconfdir}" | sed -e "s,-,\\\\\\\\-,g")
prefix_e = $(shell echo "${prefix}" | sed -e "s,-,\\\\\\\\-,g")

doc/man/%: doc/man/%.in
	@mkdir -p `dirname $@`
	$(AM_V_GEN)sed -e 's,/opt/syslog\\*-ng/etc,$(sysconfdir_e),g' -e 's,/opt/syslog\\*-ng/,$(prefix_e)/,g' <$< >$@
