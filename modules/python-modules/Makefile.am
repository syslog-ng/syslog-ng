EXTRA_DIST += \
	modules/python-modules/syslogng/__init__.py				\
	modules/python-modules/syslogng/confgen.py				\
	modules/python-modules/syslogng/dest.py					\
	modules/python-modules/syslogng/logger.py				\
	modules/python-modules/syslogng/message.py				\
	modules/python-modules/syslogng/parser.py				\
	modules/python-modules/syslogng/persist.py				\
	modules/python-modules/syslogng/reloc.py				\
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
	modules/python-modules/syslogng/modules/kubernetes/scl/kubernetes.conf	\
	modules/python-modules/syslogng/modules/kubernetes/__init__.py		\
	modules/python-modules/syslogng/modules/hypr/tests/test_confgen.py	\
	modules/python-modules/syslogng/modules/hypr/tests/test_hypr_audit_source.py	\
	modules/python-modules/syslogng/modules/hypr/scl/hypr.conf		\
	modules/python-modules/syslogng/modules/hypr/__init__.py		\
	modules/python-modules/syslogng/modules/s3/__init__.py		\
	modules/python-modules/syslogng/modules/s3/compressable_file_buffer.py		\
	modules/python-modules/syslogng/modules/s3/s3_object.py		\
	modules/python-modules/syslogng/modules/s3/s3_destination.py		\
	modules/python-modules/syslogng/modules/s3/scl/s3.conf		\
	modules/python-modules/setup.py						\
	modules/python-modules/README.md

if ENABLE_PYTHON

python_sysconf_module_DATA = modules/python-modules/README.md

PYMODULES_PATH = modules/python-modules
PYMODULES_BUILDDIR = $(abs_builddir)/$(PYMODULES_PATH)
PYMODULES_SRCDIR = $(top_srcdir)/modules/python-modules
PYMODULES_MANIFEST = $(PYMODULES_BUILDDIR)/install-manifest.txt
PYMODULES_ROOT_MODULE = syslogng

if ENABLE_PYTHON_MODULES
PYMODULES_BUILTINS_ONLY=0
else
PYMODULES_BUILTINS_ONLY=1
endif
export PYMODULES_BUILTINS_ONLY

PYTHON_ROOT = $(if $(DESTDIR),$(DESTDIR),/)

INSTALL_EXEC_HOOKS += pymodules-install
UNINSTALL_HOOKS += pymodules-uninstall
CLEAN_HOOKS += pymodules-clean

PYMODULES_SETUP_OPTIONS ?= --root="$(PYTHON_ROOT)" --install-lib="$(python_moduledir)"


#
# The syslogng package is installed into ${prefix}/lib/syslog-ng, while its
# requirements all installed to the normal Python site-packages directory.
#
# Therefore we need to separate the installation of both.
#
pymodules-install: pymodules-install-requirements python-venv
	(cd $(PYMODULES_SRCDIR) && $(PYTHON_VENV) setup.py \
		build --build-base="$(PYMODULES_BUILDDIR)/build" \
		egg_info --egg-base="$(PYMODULES_BUILDDIR)" \
		install --record=$(PYMODULES_MANIFEST) ${PYMODULES_SETUP_OPTIONS})
	$(install_sh_DATA) $(top_srcdir)/requirements.txt "$(PYTHON_ROOT)/$(python_moduledir)/requirements.txt"

pymodules-install-requirements: python-venv
	if [ "$(with_python_packages)" != "none" ]; then \
		(cd $(PYMODULES_SRCDIR) && \
		 mkdir -p $(PYMODULES_BUILDDIR) && \
	         $(PYTHON_VENV) setup.py egg_info --egg-base="$(PYMODULES_BUILDDIR)"  && \
		 if [ -f $(PYMODULES_BUILDDIR)/$(PYMODULES_ROOT_MODULE).egg-info/requires.txt ]; then \
			$(PYTHON_VENV) -m pip install -r $(PYMODULES_BUILDDIR)/$(PYMODULES_ROOT_MODULE).egg-info/requires.txt; \
		 fi) \
	fi

pymodules-uninstall:
	sed -e 's,^,$(PYTHON_ROOT),g' $(PYMODULES_MANIFEST) | tr '\n' '\0' | xargs -0 rm -f
	rm $(PYTHON_ROOT)/$(python_moduledir)/requirements.txt

pymodules-clean:
	rm -rf "$(PYMODULES_BUILDDIR)/build"
	rm -rf "$(PYMODULES_BUILDDIR)/$(PYMODULES_ROOT_MODULE).egg-info"
	rm -rf "$(PYMODULES_MANIFEST)"

pymodules-unit: python-venv
	$(PYTHON_VENV) -m pytest --showlocals --verbosity=3 $(PYMODULES_SRCDIR) -W ignore::UserWarning

pymodules-checks: pymodules-unit pymodules-pycodestyle pymodules-pylint

pymodules-pycodestyle: python-venv
	$(PYTHON_VENV) -m pycodestyle --ignore=E501,E121,E123,E126,E226,E24,E704,W503,W504 $(PYMODULES_SRCDIR)/$(PYMODULES_ROOT_MODULE)

pymodules-pylint: python-venv
	$(PYTHON_VENV) -m pylint -r n --rcfile=$(PYMODULES_SRCDIR)/pylintrc $(PYMODULES_SRCDIR)/$(PYMODULES_ROOT_MODULE)

pymodules-linters: pymodules-pycodestyle pymodules-pylint

check_SCRIPTS += modules/python-modules/test_pymodules.sh


endif

.PHONY: pymodules-install
.PHONY: pymodules-uninstall
.PHONY: pymodules-clean
.PHONY: pymodules-install-requirements
