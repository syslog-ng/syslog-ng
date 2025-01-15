SCL_SUBDIRS	= \
	apache		\
	arr		\
	cee		\
	cim		\
	cisco		\
	collectd \
	checkpoint	\
	default-network-drivers	\
	darwinosl \
	discord \
	elasticsearch	\
	ewmm		\
	fortigate	\
	google	\
	graphite	\
	graylog2	\
	hdfs		\
	iptables	\
	jellyfin	\
	junos		\
	kafka		\
	linux-audit	\
	loadbalancer	\
	loggly		\
	logmatic	\
	logscale	\
	mariadb		\
	mbox		\
	netskope	\
	nodejs		\
	openobserve \
	opensearch	\
	osquery	\
	pacct		\
	paloalto 	\
	pihole		\
	pgsql 	\
	python		\
	qbittorrent	\
	rewrite	\
	slack \
	snmptrap	\
	solaris	\
	splunk \
	sudo		\
	sumologic	\
	syslogconf	\
	system		\
	websense	\
	telegram	\
	windowseventlog \
	azure


SCL_CONFIGS	= scl.conf syslog-ng.conf

EXTRA_DIST	+= $(addprefix scl/,$(SCL_CONFIGS) $(SCL_SUBDIRS)) scl/CMakeLists.txt

scl-install-data-local:
	if [ -f $(DESTDIR)/$(sysconfdir)/syslog-ng.conf ]; then \
		echo "Not overwriting existing configuration file, you might want to upgrade manually: syslog-ng.conf"; \
	else \
		$(install_sh_DATA) $(srcdir)/scl/syslog-ng.conf  $(DESTDIR)/$(sysconfdir)/syslog-ng.conf; \
	fi
	$(mkinstalldirs) $(DESTDIR)/$(scldir)
	if [ -f $(DESTDIR)/$(config_includedir)/scl.conf ]; then \
		echo "Not overwriting existing configuration file, you might want to upgrade manually: scl.conf"; \
	else \
		$(install_sh_DATA) $(srcdir)/scl/scl.conf  $(DESTDIR)/$(config_includedir)/scl.conf; \
	fi
	(cd $(srcdir)/scl; tar cf - $(SCL_SUBDIRS)) | (cd $(DESTDIR)/$(scldir) && tar xf - --no-same-owner)
	chmod -R u+rwX $(DESTDIR)/$(scldir)

scl-uninstall-local:
	rm -f $(DESTDIR)/$(sysconfdir)/syslog-ng.conf
	rm -f $(DESTDIR)/$(config_includedir)/scl.conf
	rm -rf $(DESTDIR)/$(scldir)

scl-install: scl-install-data-local

INSTALL_EXEC_HOOKS	+= scl-install-data-local
UNINSTALL_HOOKS	+= scl-uninstall-local
