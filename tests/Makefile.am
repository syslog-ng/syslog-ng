EXTRA_DIST += tests/collect-cov.sh

coverage:
	@$(top_srcdir)/tests/collect-cov.sh

coverage-clean:
	@find $(top_builddir) -name \*.gcda | xargs rm -f

include tests/unit/Makefile.am
include tests/loggen/Makefile.am
include tests/functional/Makefile.am
