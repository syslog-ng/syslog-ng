export top_srcdir

pytest-self-check:
	@${PYTHON} -m pytest $(top_srcdir)/tests/python_functional/src

pytest-check:
	@${PYTHON} -m pytest $(top_srcdir)/tests/python_functional/functional_tests/ --installdir=${prefix}

.PHONY: pytest-self-check pytest-check
