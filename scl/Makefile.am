SCL_SUBDIRS	= \
	apache		\
	cim		\
	cisco		\
	collectd \
	default-network-drivers	\
	elasticsearch	\
	ewmm		\
	graphite	\
	graylog2	\
	hdfs		\
	iptables	\
	kafka		\
	linux-audit	\
	loadbalancer	\
	loggly		\
	logmatic	\
	mbox		\
	netskope	\
	nodejs		\
	osquery	\
	pacct		\
	rewrite	\
	slack \
	snmptrap	\
	solaris	\
	sudo		\
	syslogconf	\
	system		\
	websense	\
	telegram	\
	windowseventlog


SCL_CONFIGS	= scl.conf syslog-ng.conf

EXTRA_DIST	+= $(addprefix scl/,$(SCL_CONFIGS) $(SCL_SUBDIRS)) scl/CMakeLists.txt

scl-install-data-local:
	for cfg in $(SCL_CONFIGS); do \
		if [ -f $(DESTDIR)/$(sysconfdir)/$${cfg} ]; then \
			echo "Not overwriting existing configuration file, you might want to upgrade manually: $${cfg}"; \
		else \
			$(install_sh_DATA) $(srcdir)/scl/$${cfg}  $(DESTDIR)/$(sysconfdir)/$${cfg}; \
		fi; \
	done
	$(mkinstalldirs) $(DESTDIR)/$(scldir)
	(cd $(srcdir)/scl; tar cf - $(SCL_SUBDIRS)) | (cd $(DESTDIR)/$(scldir) && tar xf -)
	chmod -R u+rwX $(DESTDIR)/$(scldir)

scl-uninstall-local:
	for cfg in $(SCL_CONFIGS); do \
		rm -f $(DESTDIR)/$(sysconfdir)/$${cfg}; \
	done
	rm -rf $(DESTDIR)/$(scldir)

INSTALL_EXEC_HOOKS	+= scl-install-data-local
UNINSTALL_HOOKS	+= scl-uninstall-local
