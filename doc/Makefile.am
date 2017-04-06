xsd_DATA	= \
		doc/xsd/patterndb-1.xsd \
		doc/xsd/patterndb-2.xsd \
		doc/xsd/patterndb-3.xsd \
		doc/xsd/patterndb-4.xsd \
		doc/xsd/patterndb-5.xsd

EXTRA_DIST	+=  \
		doc/man/syslog-ng.8.xml \
		doc/man/syslog-ng.conf.5.xml \
		doc/man/pdbtool.1.xml \
		doc/man/loggen.1.xml \
		doc/man/syslog-ng-ctl.1.xml \
		doc/security/bof-2002-09-27.txt \
		doc/security/dos-2000-11-22.txt \
		$(xsd_DATA)


if ENABLE_MANPAGES
man_MANS	+= \
		doc/man/syslog-ng.8 \
		doc/man/syslog-ng.conf.5 \
		doc/man/pdbtool.1 \
		doc/man/loggen.1 \
		doc/man/syslog-ng-ctl.1

EXTRA_DIST	+= $(man_MANS)

# NOTE: this uses a hard-coded path for the XSL stylesheets, but the
# end-result is also included in the tarball. If so need be, this can
# be overridden from the make command line or via the environment.

sysconfdir_e = $(shell echo "${sysconfdir}" | sed -e "s,-,\\\\\\\\-,g")
prefix_e = $(shell echo "${prefix}" | sed -e "s,-,\\\\\\\\-,g")

doc/man/%: doc/man/%.xml
	$(AM_V_GEN)xsltproc --xinclude --output $@ ${XSL_STYLESHEET} $<
	$(AM_V_at)sed -e 's,/opt/syslog\\*-ng/etc,$(sysconfdir_e),g' -e 's,/opt/syslog\\*-ng/,$(prefix_e)/,g' <$@ >$@.tmp && mv $@.tmp $@
endif
