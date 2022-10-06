EXTRA_DIST += \
	modules/python-modules/syslogng/__init__.py				\
	modules/python-modules/syslogng/dest.py					\
	modules/python-modules/syslogng/logger.py				\
	modules/python-modules/syslogng/message.py				\
	modules/python-modules/syslogng/parser.py				\
	modules/python-modules/syslogng/persist.py				\
	modules/python-modules/syslogng/source.py				\
	modules/python-modules/syslogng/template.py				\
	modules/python-modules/syslogng/debuggercli/__init__.py			\
	modules/python-modules/syslogng/debuggercli/choicecompleter.py		\
	modules/python-modules/syslogng/debuggercli/commandlinelexer.py		\
	modules/python-modules/syslogng/debuggercli/completer.py		\
	modules/python-modules/syslogng/debuggercli/completerlang.py		\
	modules/python-modules/syslogng/debuggercli/debuglang.py		\
	modules/python-modules/syslogng/debuggercli/debuggercli.py		\
	modules/python-modules/syslogng/debuggercli/editline.py 		\
	modules/python-modules/syslogng/debuggercli/getoptlexer.py		\
	modules/python-modules/syslogng/debuggercli/langcompleter.py		\
	modules/python-modules/syslogng/debuggercli/lexer.py			\
	modules/python-modules/syslogng/debuggercli/lexertoken.py		\
	modules/python-modules/syslogng/debuggercli/macrocompleter.py		\
	modules/python-modules/syslogng/debuggercli/readline.py			\
	modules/python-modules/syslogng/debuggercli/syslognginternals.py	\
	modules/python-modules/syslogng/debuggercli/tablexer.py			\
	modules/python-modules/syslogng/debuggercli/templatelang.py		\
	modules/python-modules/syslogng/debuggercli/templatelexer.py		\
	modules/python-modules/syslogng/debuggercli/tflang.py			\
	modules/python-modules/syslogng/debuggercli/tests/__init__.py		\
	modules/python-modules/syslogng/debuggercli/tests/test_completer.py    	\
	modules/python-modules/syslogng/debuggercli/tests/test_commandlinelexer.py\
	modules/python-modules/syslogng/debuggercli/tests/test_completerlang.py	\
	modules/python-modules/syslogng/debuggercli/tests/test_choicecompleter.py\
	modules/python-modules/syslogng/debuggercli/tests/test_debuglang.py	\
	modules/python-modules/syslogng/debuggercli/tests/test_debuggercli.py	\
	modules/python-modules/syslogng/debuggercli/tests/test_getoptlexer.py	\
	modules/python-modules/syslogng/debuggercli/tests/test_langcompleter.py	\
	modules/python-modules/syslogng/debuggercli/tests/test_lexer.py		\
	modules/python-modules/syslogng/debuggercli/tests/test_macrocompleter.py\
	modules/python-modules/syslogng/debuggercli/tests/test_tablexer.py	\
	modules/python-modules/syslogng/debuggercli/tests/test_templatelexer.py	\
	modules/python-modules/syslogng/debuggercli/tests/test_templatelang.py	\
	modules/python-modules/syslogng/debuggercli/tests/test_tflang.py	\
	modules/python-modules/pylintrc						\
	modules/python-modules/test_pymodules.sh				\
	modules/python-modules/CMakeLists.txt					\
	\
	modules/python-modules/syslogng/modules/example/scl/example.conf	\
	modules/python-modules/syslogng/modules/example/__init__.py		\
	modules/python-modules/setup.py						\
	modules/python-modules/README.md

if ENABLE_PYTHON

python_sysconf_module_DATA = modules/python-modules/README.md

PYMODULES_PATH = modules/python-modules
PYMODULES_BUILDDIR = $(abs_builddir)/$(PYMODULES_PATH)
PYMODULES_SRCDIR = $(top_srcdir)/modules/python-modules
PYMODULES_MANIFEST = $(PYMODULES_BUILDDIR)/install-manifest.txt
PYMODULES_ROOT_MODULE = syslogng
PYTHON_ROOT = $(if $(DESTDIR),$(DESTDIR),/)

INSTALL_EXEC_HOOKS += pymodules-install
UNINSTALL_HOOKS += pymodules-uninstall
CLEAN_HOOKS += pymodules-clean

PYMODULES_SETUP_OPTIONS ?= --root="$(PYTHON_ROOT)" --install-lib="$(python_moduledir)"
IS_PYTEST_INSTALLED := $(shell ${PYTHON} -m pytest --version > /dev/null 2>&1 ; echo $$?)


#
# The syslogng package is installed into ${prefix}/lib/syslog-ng, while its
# requirements all installed to the normal Python site-packages directory.
#
# Therefore we need to separate the installation of both.
#
pymodules-install: pymodules-install-requirements
	(cd $(PYMODULES_SRCDIR) && $(PYTHON) setup.py \
		build --build-base="$(PYMODULES_BUILDDIR)/build" \
		egg_info --egg-base="$(PYMODULES_BUILDDIR)" \
		install --record=$(PYMODULES_MANIFEST) ${PYMODULES_SETUP_OPTIONS})

pymodules-install-requirements:
	(cd $(PYMODULES_SRCDIR) && \
	 mkdir -p $(PYMODULES_BUILDDIR) && \
         $(PYTHON) setup.py egg_info --egg-base="$(PYMODULES_BUILDDIR)"  && \
         if [ -f $(PYMODULES_BUILDDIR)/$(PYMODULES_ROOT_MODULE).egg-info/requires.txt ]; then $(PYTHON) -m pip install -r $(PYMODULES_BUILDDIR)/$(PYMODULES_ROOT_MODULE).egg-info/requires.txt; fi)

pymodules-uninstall:
	sed -e 's,^,$(PYTHON_ROOT),g' $(PYMODULES_MANIFEST) | tr '\n' '\0' | xargs -0 rm -f

pymodules-clean:
	rm -rf "$(PYMODULES_BUILDDIR)/build"
	rm -rf "$(PYMODULES_BUILDDIR)/$(PYMODULES_ROOT_MODULE).egg-info"
	rm -rf "$(PYMODULES_MANIFEST)"

pymodules-unit:
	@if [ $(IS_PYTEST_INSTALLED) -eq 0 ]; then \
		echo "pytest is installed"; \
		${PYTHON} -m pytest --showlocals --verbosity=3 $(PYMODULES_SRCDIR) -W ignore::UserWarning; \
	else \
		echo "pytest is not installed, can not run tests"; \
	fi;

pymodules-checks: pymodules-unit pymodules-pep8 pymodules-pylint

pymodules-pep8:
	pep8 --ignore=E501 $(PYMODULES_SRCDIR)/$(PYMODULES_ROOT_MODULE)

if PYLINT
pymodules-pylint:
	@PYLINT@ -r n --rcfile=$(PYMODULES_SRCDIR)/pylintrc $(PYMODULES_SRCDIR)/$(PYMODULES_ROOT_MODULE)
else
pymodules-pylint:
	$(error "missing pylint")
endif

check_SCRIPTS += modules/python-modules/test_pymodules.sh


endif

.PHONY: pymodules-install
.PHONY: pymodules-uninstall
.PHONY: pymodules-clean
.PHONY: pymodules-install-requirements
