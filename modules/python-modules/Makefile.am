EXTRA_DIST += \
	modules/python-modules/example/scl/example.conf	\
	modules/python-modules/example/__init__.py	\
	modules/python-modules/setup.py			\
	modules/python-modules/README.md

if ENABLE_PYTHON

PYMODULES_PATH = modules/python-modules
PYMODULES_BUILDDIR = $(abs_builddir)/$(PYMODULES_PATH)
PYMODULES_SRCDIR = $(top_srcdir)/modules/python-modules
PYMODULES_MANIFEST = $(PYMODULES_BUILDDIR)/install-manifest.txt

# PYTHON_ROOT is exported by modules/python/pylib/Makefile

INSTALL_EXEC_HOOKS += pymodules-install
UNINSTALL_HOOKS += pymodules-uninstall
CLEAN_HOOKS += pymodules-clean

PYMODULES_SETUP_OPTIONS ?= --root="$(PYTHON_ROOT)" --install-lib="$(python_moduledir)"


#
# syslogng_modules is installed into ${prefix}/lib/syslog-ng, while its
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
         $(PYTHON) -m pip install -r $(PYMODULES_BUILDDIR)/syslogng_modules.egg-info/requires.txt)

pymodules-uninstall:
	sed -e 's,^,$(PYTHON_ROOT),g' $(PYMODULES_MANIFEST) | tr '\n' '\0' | xargs -0 rm -f

pymodules-clean:
	rm -rf "$(PYMODULES_BUILDDIR)/build"
	rm -rf "$(PYMODULES_BUILDDIR)/syslogng_modules.egg-info"
	rm -rf "$(PYMODULES_MANIFEST)"

pymodules-unit:
	@if [ $(IS_PYTEST_INSTALLED) -eq 0 ]; then \
		echo "pytest is installed"; \
		${PYTHON} -m pytest --showlocals --verbosity=3 $(PYMODULES_SRCDIR) -W ignore::UserWarning; \
	else \
		echo "pytest is not installed, can not run tests"; \
	fi;

endif

.PHONY: pymodules-install
.PHONY: pymodules-uninstall
.PHONY: pymodules-clean
.PHONY: pymodules-install-requirements
