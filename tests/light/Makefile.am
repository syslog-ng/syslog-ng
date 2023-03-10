export top_srcdir

EXTRA_DIST += \
	tests/light/CMakeLists.txt \
	tests/light/conftest.py \
	tests/light/Makefile.am \
	tests/light/pytest.ini \
	tests/light/shared_files \
	tests/light/tox.ini

include tests/light/functional_tests/Makefile.am
include tests/light/src/Makefile.am

PYTEST_SUBDIR=
PYTEST_VERBOSE=false
PYTEST_OPTS=

light-self-check pytest-self-check: python-venv
	@$(PYTHON_VENV) -m pytest $(top_srcdir)/tests/light/src --showlocals --verbosity=3

light-check pytest-check: python-venv
	@$(PYTHON_VENV) -m pytest -o log_cli=$(PYTEST_VERBOSE) $(top_srcdir)/tests/light/functional_tests/$(PYTEST_SUBDIR) $(PYTEST_OPTS) --installdir=${prefix} --showlocals --verbosity=3 --show-capture=no

light-linters pytest-linters: python-venv
	@find $(top_srcdir)/tests/light/ -name "*.py" \
		-not -path "*reports*" \
		-not -path "*tox*" \
		-print0 | xargs -0 \
		$(PYTHON_VENV) -m pre_commit run --show-diff-on-failure --config=$(top_srcdir)/tests/light/.pre-commit-config.yaml --files

.PHONY: light-self-check light-check light-linters pytest-self-check pytest-check pytest-linters
