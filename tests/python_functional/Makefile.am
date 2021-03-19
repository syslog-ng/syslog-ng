export top_srcdir

EXTRA_DIST += \
	tests/python_functional/CMakeLists.txt \
	tests/python_functional/conftest.py \
	tests/python_functional/Makefile.am \
	tests/python_functional/pytest.ini \
	tests/python_functional/shared_files \
	tests/python_functional/tox.ini

include tests/python_functional/functional_tests/Makefile.am
include tests/python_functional/src/Makefile.am

PYTEST_SUBDIR=
PYTEST_VERBOSE=false

pytest-self-check:
	@${PYTHON} -m pytest $(top_srcdir)/tests/python_functional/src --showlocals --verbosity=3

pytest-check:
	@${PYTHON} -m pytest -o log_cli=$(PYTEST_VERBOSE) $(top_srcdir)/tests/python_functional/functional_tests/$(PYTEST_SUBDIR) --installdir=${prefix} --showlocals --verbosity=3 --show-capture=no

pytest-linters:
	@find $(top_srcdir)/tests/python_functional/ -name "*.py" \
		-not -path "*reports*" \
		-not -path "*tox*" \
		-print0 | xargs -0 \
		pre-commit run --show-diff-on-failure --config=$(top_srcdir)/tests/python_functional/.pre-commit-config.yaml --files

.PHONY: pytest-self-check pytest-check pytest-linters
