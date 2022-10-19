EXTRA_DIST += \
	modules/python-modules/example/scl/example.conf	\
	modules/python-modules/example/__init__.py	\
	modules/python-modules/setup.py

if ENABLE_PYTHON

PYMODULES_PATH = modules/python-modules
PYMODULES_BUILDDIR = $(abs_builddir)/$(PYMODULES_PATH)
PYMODULES_SRCDIR = $(top_srcdir)/modules/python-modules

# PYTHON_ROOT is exported by modules/python/pylib/Makefile

INSTALL_EXEC_HOOKS += pymodules-install
UNINSTALL_HOOKS += pymodules-uninstall
CLEAN_HOOKS += pymodules-clean

#
# syslogng_modules is installed into ${prefix}/lib/syslog-ng, while its
# requirements all installed to the normal Python site-packages directory.
#
# Therefore we need to separate the installation of both.
#
pymodules-install: pymodules-install-requirements
	(cd $(PYMODULES_SRCDIR) && $(PYTHON) -m pip install --no-deps --upgrade --target="$(PYTHON_ROOT)/$(python_moduledir)" .)

# install requirements where pip otherwise would install it (e.g.  no
# --target option).  This means we are using the venv if one is active or
# system site-packages otherwise)
#
pymodules-install-requirements:
	(cd $(PYMODULES_SRCDIR) && \
	 mkdir -p $(PYMODULES_BUILDDIR) && \
         $(PYTHON) setup.py egg_info --egg-base $(PYMODULES_BUILDDIR)  && \
         $(PYTHON) -m pip install -r $(PYMODULES_BUILDDIR)/syslogng_modules.egg-info/requires.txt)

pymodules-uninstall:
	(cd $(PYMODULES_SRCDIR) && PYTHONPATH="$(PYTHON_ROOT)/$(python_moduledir):$$PYTHONPATH" $(PYTHON) -m pip uninstall -y syslogng-modules)

pymodules-clean:
	rm -rf "$(PYMODULES_BUILDDIR)/build"

endif

.PHONY: pymodules-install
.PHONY: pymodules-uninstall
.PHONY: pymodules-clean
.PHONY: pymodules-install-requirements
