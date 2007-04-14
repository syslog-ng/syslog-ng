
EXTRA_DIST =  man/syslog-ng.8 man/syslog-ng.conf.5 \
	security/bof-2002-09-27.txt \
	security/dos-2000-11-22.txt \
	reference/syslog-ng.html.tar.gz \
	reference/syslog-ng.xml \
	reference/syslog-ng.txt \
	reference/syslog-ng.xsl \
	examples/syslog-ng.conf.sample \
	examples/syslog-ng.conf.solaris

man_MANS = man/syslog-ng.8 man/syslog-ng.conf.5

reference/syslog-ng.html.tar.gz: reference/syslog-ng.xml docvars.xml
	(set -e; 						\
	mkdir reference || /bin/true;				\
	xsltproc --nonet --path $(top_builddir)/doc -o reference/syslog-ng.html/index.html 	\
		$(top_srcdir)/doc/reference/syslog-ng.xsl 	\
		$(top_srcdir)/doc/reference/syslog-ng.xml;	\
	cd reference; 						\
	tar cfz syslog-ng.html.tar.gz syslog-ng.html)

reference/syslog-ng.txt: reference/syslog-ng.html.tar.gz
	w3m -dump reference/syslog-ng.html/index.html > $@
