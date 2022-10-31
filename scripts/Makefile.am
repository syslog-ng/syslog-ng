bin_SCRIPTS	+= scripts/update-patterndb scripts/syslog-ng-update-virtualenv
EXTRA_DIST	+= scripts/update-patterndb.in \
	scripts/CMakeLists.txt	\
	scripts/get-libjvm-path.sh	\
	scripts/make-and-check.sh	\
	scripts/rm-extra-msg-sentinel.sh	\
	scripts/version.sh	\
	scripts/test-grep.sh	\
	scripts/style-checker.sh	\
	scripts/build-python-venv.sh \
	scripts/syslog-ng-update-virtualenv.in


install-exec-local:
	$(mkinstalldirs) $(DESTDIR)/$(sysconfdir)/patterndb.d
