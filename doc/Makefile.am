man_MANS = man/syslog-ng.8 man/syslog-ng.conf.5 man/pdbtool.1 man/loggen.1 man/syslog-ng-ctl.1

EXTRA_DIST =  man/syslog-ng.8.xml man/syslog-ng.conf.5.xml man/pdbtool.1.xml man/loggen.1.xml man/syslog-ng-ctl.1.xml \
	$(man_MANS) \
	security/bof-2002-09-27.txt \
	security/dos-2000-11-22.txt \
	xsd/patterndb-1.xsd \
	xsd/patterndb-2.xsd \
	xsd/patterndb-3.xsd \
	xsd/patterndb-4.xsd

# NOTE: this uses a hard-coded path for the XSL stylesheets, but the
# end-result is also included in the tarball.  Might need to detect this
# from the configure script.

XSLTPROC_MANPAGE=xsltproc --xinclude --output $@  /usr/share/xml/docbook/stylesheet/docbook-xsl/manpages/docbook.xsl $< | sed -e 's,/opt/syslog-ng/etc,$(sysconfdir),g' -e 's,/opt/syslog-ng/,$(prefix),g'

xsddir = $(datadir)/xsd
xsd_DATA = xsd/patterndb-1.xsd xsd/patterndb-2.xsd xsd/patterndb-3.xsd

man/%.1: man/%.1.xml
	$(XSLTPROC_MANPAGE)

man/%.2: man/%.2.xml
	$(XSLTPROC_MANPAGE)

man/%.3: man/%.3.xml
	$(XSLTPROC_MANPAGE)

man/%.4: man/%.4.xml
	$(XSLTPROC_MANPAGE)

man/%.5: man/%.5.xml
	$(XSLTPROC_MANPAGE)

man/%.6: man/%.6.xml
	$(XSLTPROC_MANPAGE)

man/%.7: man/%.7.xml
	$(XSLTPROC_MANPAGE)

man/%.8: man/%.8.xml
	$(XSLTPROC_MANPAGE)
