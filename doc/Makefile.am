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
		doc/man/update-manpages.sh \
		doc/security/bof-2002-09-27.txt \
		doc/security/dos-2000-11-22.txt \
		doc/xsd/examples/patterndb-1.xml \
		$(xsd_DATA)

if ENABLE_MANPAGES
man_MANS	+= \
		doc/man/dqtool.1 \
		doc/man/loggen.1 \
		doc/man/pdbtool.1 \
		doc/man/persist-tool.1 \
		doc/man/syslog-ng-debun.1 \
		doc/man/syslog-ng-ctl.1 \
		doc/man/syslog-ng.8 \
		doc/man/syslog-ng.conf.5

EXTRA_DIST	+= $(man_MANS)

# NOTE: this uses a hard-coded path for the XSL stylesheets, but the
# end-result is also included in the tarball. If so need be, this can
# be overridden from the make command line or via the environment.

sysconfdir_e = $(shell echo "${sysconfdir}" | sed -e "s,-,\\\\\\\\-,g")
prefix_e = $(shell echo "${prefix}" | sed -e "s,-,\\\\\\\\-,g")

doc/man/%: doc/man/%.xml
	$(AM_V_at) $(NOOP) # Removing all XML whitespaces, workaround for the bug in
	$(AM_V_at) $(NOOP) # docbook-xsl v1.79.1. See https://sourceforge.net/p/docbook/bugs/1381/
	$(AM_V_GEN)sed -e '1h;2,$$H;$$!d;g' -e "s/>\s\+</></g" <$< | xsltproc --xinclude --output $@ ${XSL_STYLESHEET} -
	$(AM_V_at)sed -e 's,/opt/syslog\\*-ng/etc,$(sysconfdir_e),g' -e 's,/opt/syslog\\*-ng/,$(prefix_e)/,g' <$@ >$@.tmp && mv $@.tmp $@
endif
