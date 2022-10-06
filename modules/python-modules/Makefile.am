EXTRA_DIST += \
	modules/python-modules/example/scl/example.conf	\
	modules/python-modules/example/__init__.py	\
	modules/python-modules/setup.py

if ENABLE_PYTHON

PYMODULES_PATH = modules/python-modules
PYMODULES_BUILDDIR = $(abs_builddir)/$(PYMODULES_PATH)
PYMODULES_SRCDIR = $(top_srcdir)/modules/python-modules

# PYTHON_ROOT is exported by modules/python/pylib/Makefile

INSTALL_EXEC_HOOKS += install-pymodules
UNINSTALL_HOOKS += uninstall-pymodules
CLEAN_HOOKS += clean-pymodules

PYMODULES_SETUP_OPTIONS ?= --root="$(PYTHON_ROOT)" --install-lib="$(python_moduledir)"

#
# syslogng_modules is installed into ${prefix}/lib/syslog-ng, while its
# requirements all installed to the normal Python site-packages directory.
#
# Therefore we need to separate the installation of both.
#
install-pymodules: install-pymodules-requirements
	(cd $(PYMODULES_SRCDIR) && $(PYTHON) -m pip install --no-deps --upgrade --target="$(PYTHON_ROOT)/$(python_moduledir)" .)

# install requirements where pip otherwise would install it (e.g.
# venv if that's set or system site-packages otherwise)
#
# Unfortunately I couldn't use the install_requires option in setup.py to
# describe these dependencies as pip insists on installing requirements
# along with the package itself and we are installing the requirements to
# the standard locations, while installing our own modules into
# ${prefix}/lib/syslog-ng/python
#
install-pymodules-requirements:
	(cd $(PYMODULES_SRCDIR) && \
         $(PYTHON) setup.py egg_info --egg-base $(PYMODULES_BUILDDIR)  && \
         $(PYTHON) -m pip install -r $(PYMODULES_BUILDDIR)/syslogng_modules.egg-info/requires.txt)

uninstall-pymodules:
	(cd $(PYMODULES_SRCDIR) && PYTHONPATH="$(PYTHON_ROOT)/$(python_moduledir):$$PYTHONPATH" $(PYTHON) -m pip uninstall -y syslogng-modules)

clean-pymodules:
	rm -rf "$(PYMODULES_BUILDDIR)/build"

endif

.PHONY: install-pymodules
.PHONY: uninstall-pymodules
.PHONY: clean-pymodules
.PHONY: install-pymodules-requirements