SCL_SUBDIRS	= \
	system		\
	pacct		\
	syslogconf	\
	rewrite		\
	nodejs		\
	graphite	\
	cim		\
	solaris		\
	mbox		\
	elasticsearch	\
	kafka		\
	hdfs		\
	apache		\
	cisco		\
	default-network-drivers		\
	loggly		\
	logmatic	\
	snmptrap        \
	osquery	        \
        windowseventlog  \
        loadbalancer

SCL_CONFIGS	= scl.conf syslog-ng.conf

EXTRA_DIST	+= $(addprefix scl/,$(SCL_CONFIGS) $(SCL_SUBDIRS))

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
