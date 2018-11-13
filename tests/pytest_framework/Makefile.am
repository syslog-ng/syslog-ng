export top_srcdir

pytest-self-check:
	@${PYTHON} -m pytest $(top_srcdir)/tests/pytest_framework/src

pytest-check:
	@${PYTHON} -m pytest $(top_srcdir)/tests/pytest_framework/functional_tests/ --installdir=${prefix}

.PHONY: pytest-self-check pytest-check
