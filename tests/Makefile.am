EXTRA_DIST += tests/collect-cov.sh

check-copyright:
	@$(top_srcdir)/tests/copyright/check.sh $(top_srcdir) $(top_builddir) policy

coverage:
	@$(top_srcdir)/tests/collect-cov.sh

coverage-clean:
	@find $(top_builddir) -name \*.gcda | xargs rm -f

include tests/unit/Makefile.am
include tests/loggen/Makefile.am
include tests/functional/Makefile.am
