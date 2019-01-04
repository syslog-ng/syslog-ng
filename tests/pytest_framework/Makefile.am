export top_srcdir

pytest-self-check:
	@${PYTHON} -m pytest $(top_srcdir)/tests/pytest_framework/src

pytest-check:
	@${PYTHON} -m pytest $(top_srcdir)/tests/pytest_framework/functional_tests/ --installdir=${prefix}

pytest-check-all:
	@${PYTHON} -m pytest $(top_srcdir)/tests/pytest_framework/functional_tests/ --runslow --installdir=${prefix}

.PHONY: pytest-self-check pytest-check pytest-check-all
