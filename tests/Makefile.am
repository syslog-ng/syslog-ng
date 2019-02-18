EXTRA_DIST += \
	tests/collect-cov.sh \
	tests/copyright/check.sh \
	tests/copyright/policy \
	tests/copyright/license.text.GPLv2+.txt \
	tests/copyright/license.text.GPLv2+_SSL.txt \
	tests/copyright/license.text.LGPLv2.1+.txt \
	tests/copyright/license.text.LGPLv2.1+_SSL.txt \
	tests/valgrind/unit-test-leak.supp	\
	tests/build-log-cflags-propagation.sh	\
	tests/CMakeLists.txt

check-copyright:
	@cd $(top_srcdir) && ./tests/copyright/check.sh . $(abs_builddir) policy

CLEAN_HOOKS += clean-check-copyright

clean-check-copyright:
	rm -f copyright-run.log copyright-err.log

coverage:
	@$(top_srcdir)/tests/collect-cov.sh

coverage-clean:
	@find $(top_builddir) -name \*.gcda | xargs rm -f

include tests/unit/Makefile.am
include tests/loggen/Makefile.am
include tests/functional/Makefile.am
include tests/python_functional/Makefile.am
