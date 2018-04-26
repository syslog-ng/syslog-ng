PYTHON_VENV_DIR = $(abs_builddir)/.venv

# can be used to reference to the Python interpreter, preferred from our
# virtualenv should one be present.  The $(PYTHON) variable always
# references the system Python.  Note that the libpython-dev detection uses
# pkg-config from the configure script and they should match up with the
# virtualenv.

VPYTHON = $(or $(wildcard $(PYTHON_VENV_DIR)/bin/python),$(PYTHON))
PYLINT = $(or $(wildcard $(PYTHON_VENV_DIR)/bin/pylint),$(shell which pylint))
PEP8 = $(or $(wildcard $(PYTHON_VENV_DIR)/bin/pycodestyle),$(wildcard $(PYTHON_VENV_DIR)/bin/pep8),$(shell which pycodestyle),$(shell which pep8))
NOSETESTS = $(or $(wildcard $(PYTHON_VENV_DIR)/bin/nosetests),$(shell which nosetests))


EXTRA_DIST	+= Mk/lex-rules.am \
		Mk/find-top-builddir.sh \
		Mk/populate-makefiles.sh \
		Mk/subdir.mk

tools_DATA	= \
		Mk/lex-rules.am \
		lib/cfg-grammar.y

tools_SCRIPTS	= \
		lib/merge-grammar.py

python-venv:
	rm -rf $(PYTHON_VENV_DIR)
	virtualenv --python=$(PYTHON) ${PYTHON_VENV_DIR}
	$(PYTHON_VENV_DIR)/bin/pip install -r $(top_srcdir)/requirements.txt
