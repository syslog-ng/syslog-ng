EXTRA_DIST += \
	modules/python/pylib/syslogng/__init__.py				\
	modules/python/pylib/syslogng/dest.py					\
	modules/python/pylib/syslogng/logger.py					\
	modules/python/pylib/syslogng/message.py				\
	modules/python/pylib/syslogng/parser.py					\
	modules/python/pylib/syslogng/persist.py				\
	modules/python/pylib/syslogng/source.py					\
	modules/python/pylib/syslogng/template.py				\
	modules/python/pylib/syslogng/debuggercli/__init__.py			\
	modules/python/pylib/syslogng/debuggercli/choicecompleter.py		\
	modules/python/pylib/syslogng/debuggercli/commandlinelexer.py		\
	modules/python/pylib/syslogng/debuggercli/completer.py			\
	modules/python/pylib/syslogng/debuggercli/completerlang.py		\
	modules/python/pylib/syslogng/debuggercli/debuglang.py			\
	modules/python/pylib/syslogng/debuggercli/debuggercli.py		\
	modules/python/pylib/syslogng/debuggercli/editline.py \
	modules/python/pylib/syslogng/debuggercli/getoptlexer.py		\
	modules/python/pylib/syslogng/debuggercli/langcompleter.py		\
	modules/python/pylib/syslogng/debuggercli/lexer.py			\
	modules/python/pylib/syslogng/debuggercli/lexertoken.py			\
	modules/python/pylib/syslogng/debuggercli/macrocompleter.py		\
	modules/python/pylib/syslogng/debuggercli/readline.py			\
	modules/python/pylib/syslogng/debuggercli/syslognginternals.py		\
	modules/python/pylib/syslogng/debuggercli/tablexer.py			\
	modules/python/pylib/syslogng/debuggercli/templatelang.py		\
	modules/python/pylib/syslogng/debuggercli/templatelexer.py		\
	modules/python/pylib/syslogng/debuggercli/tflang.py			\
	modules/python/pylib/syslogng/debuggercli/tests/__init__.py		\
	modules/python/pylib/syslogng/debuggercli/tests/test_completer.py    	\
	modules/python/pylib/syslogng/debuggercli/tests/test_commandlinelexer.py	\
	modules/python/pylib/syslogng/debuggercli/tests/test_completerlang.py	\
	modules/python/pylib/syslogng/debuggercli/tests/test_choicecompleter.py	\
	modules/python/pylib/syslogng/debuggercli/tests/test_debuglang.py	\
	modules/python/pylib/syslogng/debuggercli/tests/test_debuggercli.py	\
	modules/python/pylib/syslogng/debuggercli/tests/test_getoptlexer.py	\
	modules/python/pylib/syslogng/debuggercli/tests/test_langcompleter.py	\
	modules/python/pylib/syslogng/debuggercli/tests/test_lexer.py		\
	modules/python/pylib/syslogng/debuggercli/tests/test_macrocompleter.py	\
	modules/python/pylib/syslogng/debuggercli/tests/test_tablexer.py	\
	modules/python/pylib/syslogng/debuggercli/tests/test_templatelexer.py	\
	modules/python/pylib/syslogng/debuggercli/tests/test_templatelang.py	\
	modules/python/pylib/syslogng/debuggercli/tests/test_tflang.py		\
	modules/python/pylib/setup.py						\
	modules/python/pylib/pylintrc						\
	modules/python/pylib/test_pylib.sh	\
	modules/python/pylib/CMakeLists.txt

if ENABLE_PYTHON


PYLIB_PATH = modules/python/pylib
PYLIB_BUILDDIR = $(abs_builddir)/$(PYLIB_PATH)
PYLIB_SRCDIR = $(top_srcdir)/modules/python/pylib
PYLIB_MANIFEST = $(PYLIB_BUILDDIR)/install-manifest.txt
PYTHON_ROOT = $(if $(DESTDIR),$(DESTDIR),/)

INSTALL_EXEC_HOOKS += pylib-install
UNINSTALL_HOOKS += pylib-uninstall
CLEAN_HOOKS += pylib-clean

IS_PYTEST_INSTALLED := $(shell ${PYTHON} -m pytest --version > /dev/null 2>&1 ; echo $$?)

pylib-install:
	(cd $(PYLIB_SRCDIR) && $(PYTHON) -m pip install --upgrade --target="$(PYTHON_ROOT)/$(python_moduledir)" .)

pylib-uninstall:
	(cd $(PYLIB_SRCDIR) && PYTHONPATH="$(PYTHON_ROOT)/$(python_moduledir):$$PYTHONPATH" $(PYTHON) -m pip uninstall -y syslogng)


pylib-checks: pylib-unit pylib-pep8 pylib-pylint

pylib-unit:
	@if [ $(IS_PYTEST_INSTALLED) -eq 0 ]; then \
			echo "pytest is installed"; \
			${PYTHON} -m pytest --showlocals --verbosity=3 $(PYLIB_SRCDIR)/syslogng -W ignore::UserWarning; \
	else \
			echo "pytest is not installed, can not run tests"; \
	fi;

pylib-pep8:
	pep8 --ignore=E501 $(PYLIB_SRCDIR)/syslogng

if PYLINT
pylib-pylint:
	@PYLINT@ -r n --rcfile=$(PYLIB_SRCDIR)/pylintrc $(PYLIB_SRCDIR)/syslogng
else
pylib-pylint:
	$(error "missing pylint")
endif


modules_python_pylib_tests_TESTS = modules/python/pylib/test_pylib.sh
check_SCRIPTS += $(modules_python_pylib_tests_TESTS)

endif

.PHONY: pylib-install
.PHONY: pylib-uninstall
.PHONY: pylib-clean
.PHONY: python-checks python-unit python-pep8 python-pylint
