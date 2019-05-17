export top_srcdir

PYTEST_SUBDIR=
PYTEST_VERBOSE=false

pytest-self-check:
	@${PYTHON} -m pytest $(top_srcdir)/tests/python_functional/src

pytest-check:
	@${PYTHON} -m pytest -o log_cli=$(PYTEST_VERBOSE) $(top_srcdir)/tests/python_functional/functional_tests/$(PYTEST_SUBDIR) --installdir=${prefix}

pytest-linters:
	@find $(top_srcdir)/tests/python_functional/ -name "*.py" \
		-not -path "*reports*" \
		-not -path "*tox*" \
		-print0 | xargs -0 \
		pre-commit run --show-diff-on-failure --config=$(top_srcdir)/tests/python_functional/.pre-commit-config.yaml --files

.PHONY: pytest-self-check pytest-check pytest-linters
