bin_SCRIPTS	+= scripts/update-patterndb
EXTRA_DIST	+= scripts/update-patterndb.in \
	scripts/CMakeLists.txt	\
	scripts/get-libjvm-path.sh	\
	scripts/make-and-check.sh	\
	scripts/rm-extra-msg-sentinel.sh	\
	scripts/version.sh	\
	scripts/style-checker.sh

install-exec-local:
	$(mkinstalldirs) $(DESTDIR)/$(sysconfdir)/patterndb.d
