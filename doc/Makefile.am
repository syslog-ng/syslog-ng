xsd_DATA	= \
		doc/xsd/patterndb-1.xsd \
		doc/xsd/patterndb-2.xsd \
		doc/xsd/patterndb-3.xsd \
		doc/xsd/patterndb-4.xsd \
		doc/xsd/patterndb-5.xsd

EXTRA_DIST	+=  \
		doc/man/dqtool.1.xml \
		doc/man/loggen.1.xml \
		doc/man/pdbtool.1.xml \
		doc/man/persist-tool.1.xml \
		doc/man/syslog-ng-debun.1.xml \
		doc/man/syslog-ng-ctl.1.xml \
		doc/man/syslog-ng.8.xml \
		doc/man/syslog-ng.conf.5.xml \
		doc/man/slogencrypt.1.xml \
		doc/man/slogkey.1.xml \
		doc/man/slogverify.1.xml \
		doc/man/secure-logging.7.xml \
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

if ENABLE_MANPAGES_INSTALL

man_MANS        += $(manpages)

endif

if ENABLE_MANPAGES

# we only clean manpages if we have the ability to regenerate them
CLEANFILES      += $(manpages)

sysconfdir_e = $(shell echo "${sysconfdir}" | sed -e "s,-,\\\\\\\\-,g")
prefix_e = $(shell echo "${prefix}" | sed -e "s,-,\\\\\\\\-,g")

doc/man/%: doc/man/%.xml
	$(AM_V_at) $(NOOP) # Removing all XML whitespaces, workaround for the bug in
	$(AM_V_at) $(NOOP) # docbook-xsl v1.79.1. See https://sourceforge.net/p/docbook/bugs/1381/
	$(AM_V_GEN)sed -e '1h;2,$$H;$$!d;g' -e "s/>\s\+</></g" <$< | xsltproc --xinclude --output $@ ${XSL_STYLESHEET} -
	$(AM_V_at)sed -e 's,/opt/syslog\\*-ng/etc,$(sysconfdir_e),g' -e 's,/opt/syslog\\*-ng/,$(prefix_e)/,g' <$@ >$@.tmp && mv $@.tmp $@
else

doc/man/%: doc/man/%.xml
	@echo "Error: unable to generate manual pages"
	@echo ""
	@echo "The generation manual pages was not enabled using --enable-manpages to configure and generated "
	@echo "versions do not seem to be available (which should be if you are building from a release tarball) "
	@echo "or the man page sources were changed."
	@echo ""
	@echo "Either build from an official release tarball or install documentation tools and run configure with --enable-manpages"
	@exit 1

endif
