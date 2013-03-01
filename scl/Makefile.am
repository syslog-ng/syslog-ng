SCL_SUBDIRS = system pacct syslogconf rewrite
SCL_CONFIGS = scl.conf syslog-ng.conf
EXTRA_DIST = $(SCL_CONFIGS) $(SCL_SUBDIRS)

scldir = $(datadir)/include/scl

install-data-local:
	for cfg in $(SCL_CONFIGS); do \
		if [ -f $(DESTDIR)/$(sysconfdir)/$${cfg} ]; then \
			echo "Not overwriting existing configuration file, you might want to upgrade manually: $${cfg}"; \
		else \
			$(install_sh_DATA) $(srcdir)/$${cfg}  $(DESTDIR)/$(sysconfdir)/$${cfg}; \
		fi; \
	done
	$(mkinstalldirs) $(DESTDIR)/$(scldir)
	(cd $(srcdir); tar cf - $(SCL_SUBDIRS)) | (cd $(DESTDIR)/$(scldir) && tar xf -)

uninstall-local:
	for cfg in $(SCL_CONFIGS); do \
		rm -f $(DESTDIR)/$(sysconfdir)/$${cfg}; \
	done
	rm -rf $(DESTDIR)/$(scldir)
