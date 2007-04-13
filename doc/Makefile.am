
EXTRA_DIST =  man/syslog-ng.8 man/syslog-ng.conf.5 \
	security/bof-2002-09-27.txt \
	security/dos-2000-11-22.txt \
	reference/syslog-ng.html.tar.gz \
	reference/syslog-ng.xml \
	reference/syslog-ng.xsl \
	examples/syslog-ng.conf.sample \
	examples/syslog-ng.conf.solaris

man_MANS = man/syslog-ng.8 man/syslog-ng.conf.5

reference/syslog-ng.html.tar.gz: reference/syslog-ng.xml
	(set -e; 						\
	mkdir reference || /bin/true;				\
	cd reference; 						\
	pwd;							\
	xsltproc -o syslog-ng.html/index.html 			\
		$(top_srcdir)/doc/reference/syslog-ng.xsl 	\
		$(top_srcdir)/doc/reference/syslog-ng.xml;	\
	tar cfz syslog-ng.html.tar.gz syslog-ng.html)
	