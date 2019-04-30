export top_srcdir

PYTEST_SUBDIR=
PYTEST_VERBOSE=false

pytest-self-check:
	@${PYTHON} -m pytest $(top_srcdir)/tests/python_functional/src

pytest-check:
	@${PYTHON} -m pytest -o log_cli=$(PYTEST_VERBOSE) $(top_srcdir)/tests/python_functional/functional_tests/$(PYTEST_SUBDIR) --installdir=${prefix}

.PHONY: pytest-self-check pytest-check
