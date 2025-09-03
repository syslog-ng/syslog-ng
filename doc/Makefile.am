xsd_DATA	= \
		doc/xsd/patterndb-1.xsd \
		doc/xsd/patterndb-2.xsd \
		doc/xsd/patterndb-3.xsd \
		doc/xsd/patterndb-4.xsd \
		doc/xsd/patterndb-5.xsd \
		doc/xsd/patterndb-6.xsd


EXTRA_DIST	+=  \
		doc/man/local/dqtool.1 \
		doc/man/local/loggen.1 \
		doc/man/local/pdbtool.1 \
		doc/man/local/persist-tool.1 \
		doc/man/local/secure-logging.7 \
		doc/man/local/slogencrypt.1 \
		doc/man/local/slogkey.1 \
		doc/man/local/slogverify.1 \
		doc/man/local/syslog-ng.8 \
		doc/man/local/syslog-ng.conf.5 \
		doc/man/local/syslog-ng-ctl.1 \
		doc/man/local/syslog-ng-debun.1 \
		doc/man/update-manpages.sh \
		doc/security/bof-2002-09-27.txt \
		doc/security/dos-2000-11-22.txt \
		doc/xsd/examples/patterndb-1.xml \
		$(xsd_DATA)

manpages	= \
		doc/man/dqtool.1 \
		doc/man/loggen.1 \
		doc/man/pdbtool.1 \
		doc/man/persist-tool.1 \
		doc/man/secure-logging.7 \
		doc/man/slogencrypt.1 \
		doc/man/slogkey.1 \
		doc/man/slogverify.1 \
		doc/man/syslog-ng.8 \
		doc/man/syslog-ng.conf.5 \
		doc/man/syslog-ng-ctl.1 \
		doc/man/syslog-ng-debun.1

# "dist" will always need the manpages
EXTRA_DIST += $(manpages)

if ENABLE_MANPAGES_INSTALL

man_MANS += $(manpages)

endif

if ENABLE_MANPAGES

if MANPAGES_REMOTE
PREP_CMD = sh $(top_srcdir)/doc/man/update-manpages.sh -f $(top_builddir)/doc/man
else
PREP_CMD = cp $(top_srcdir)/doc/man/local/* $(top_builddir)/doc/man/
endif

sysconfdir_e = $(shell echo "${sysconfdir}" | sed -e "s,-,\\\\\\\\-,g")
prefix_e = $(shell echo "${prefix}" | sed -e "s,-,\\\\\\\\-,g")

# 1) Prepare step: create dir + fetch/copy (runs once, creates a stamp)
doc/man/.prep:
	@echo "Preparing manpages (MANPAGES_REMOTE=$(if $(MANPAGES_REMOTE),yes,no))..."
	$(AM_V_at)$(MKDIR_P) $(top_builddir)/doc/man
	$(AM_V_at)$(PREP_CMD)
	$(AM_V_at)touch $@

# 2) Update step: run path substitutions only after prep
doc/man/.updated: $(top_srcdir)/doc/man/update-manpages.sh doc/man/.prep
	@echo "Updating manpages..."
	$(AM_V_at)sh "$(top_srcdir)/doc/man/update-manpages.sh" -u "$(sysconfdir_e)" "$(prefix_e)" "$(top_builddir)/doc/man"
	$(AM_V_at)touch $@

# 3) Any concrete manpage depends on the updated stamp
$(manpages): doc/man/.updated

# 4) all-local should ensure the update is done
all-local: doc/man/.updated

CLEANFILES += $(manpages) doc/man/.prep doc/man/.updated

endif