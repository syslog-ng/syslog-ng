man_MANS = man/syslog-ng.8 man/syslog-ng.conf.5 man/pdbtool.1 man/loggen.1 man/syslog-ng-ctl.1

EXTRA_DIST =  man/syslog-ng.8.xml man/syslog-ng.conf.5.xml man/pdbtool.1.xml man/loggen.1.xml man/syslog-ng-ctl.1.xml \
	$(man_MANS) \
	security/bof-2002-09-27.txt \
	security/dos-2000-11-22.txt \
	xsd/patterndb-1.xsd \
	xsd/patterndb-2.xsd \
	xsd/patterndb-3.xsd \
	xsd/patterndb-4.xsd

xsddir = $(datadir)/xsd
xsd_DATA = xsd/patterndb-1.xsd xsd/patterndb-2.xsd xsd/patterndb-3.xsd xsd/patterndb-4.xsd

# NOTE: this uses a hard-coded path for the XSL stylesheets, but the
# end-result is also included in the tarball. If so need be, this can
# be overridden from the make command line or via the environment.
XSL_STYLESHEET = /usr/share/xml/docbook/stylesheet/docbook-xsl/manpages/docbook.xsl

sysconfdir_e = $(shell echo "${sysconfdir}" | sed -e "s,-,\\\\\\\\-,g")
prefix_e = $(shell echo "${prefix}" | sed -e "s,-,\\\\\\\\-,g")

man/%: man/%.xml
	$(AM_V_GEN)xsltproc --xinclude --output $@ ${XSL_STYLESHEET} $<
	$(AM_V_at)sed -e 's,/opt/syslog\\*-ng/etc,$(sysconfdir_e),g' -e 's,/opt/syslog\\*-ng/,$(prefix_e)/,g' <$@ >$@.tmp && mv $@.tmp $@
