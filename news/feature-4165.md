Python implemented syslog-ng components: a framework was added to syslog-ng
that allows seamless implementation of syslog-ng features in Python, with a
look and feel of that of a native implementation. An example for using this
framework is available in the `modules/python-modules/example` directory, as
well as detailed documentation in the form of
modules/python-modules/README.md that is installed to /etc/syslog-ng/python.

`python()` typing: support for typing was added to all Python components
(sources, destinations, parsers and template functions), along with more
documentation & examples on how the Python bindings work.  All types except
json() are supported as they are queried- or changed by Python code.

`syslogng` Python module: native code provided by the syslog-ng core has
traditionally been exported in the `syslogng` Python module.  An effort
was made to make these native classes exported by the C layer more
discoverable and more intuitive.  As a part of this effort, the interfaces
for all key Python components (LogSource, LogFetcher, LogDestination, LogParser)
were exposed in the syslogng module, along with in-line documentation.

`LogMessage` class: get_pri() and get_timestamp() methods were added that
allow the query of the syslog-style priority and the message timestamp,
respectively. The return value of get_pri() is an integer, while
get_timestamp() returns a Python datetime.datetime instance. Some macros
that were previously unavailable from Python (e.g. the STAMP, R_STAMP and
C_STAMP macros) are now made available.

`/etc/syslog-ng/python`: syslog-ng now automatically adds this directory to
the PYTHONPATH so that you have an easy place to add Python modules required
by your configuration.

Python virtualenv support for production use: more sophisticated Python
modules usually have 3rd party dependencies, which either needed to be
installed from the OS repositories (using the apt-get or yum/dnf tools) or
PyPI (using the pip tool).  syslog-ng now acquired support for an embedded
Python virtualenv (/var/lib/syslog-ng/python-venv or similar, depending on
the installation layout), meaning that these requirements can be installed
privately, without deploying them in the system PYTHONPATH where it might
collide with other applications.  The base set of requirements that
syslog-ng relies on can be installed via the `syslog-ng-update-virtualenv`
script which has been added to our rpm/deb postinst scripts.

Our mod-python module validates this virtualenv at startup and activates it
automatically if the validation is successful.  You can disable this behaviour
by loading the Python module explicitly with the following configuration
statement:

        @module mod-python use-virtualenv(no)

You can force syslog-ng to use a specific virtualenv by activating it first,
prior to executing syslog-ng.  In this case, syslog-ng will not try to use
its private virtualenv, rather it would use the one activated when it was
started.  It assumes that any requirements needed for syslog-ng
functionality implemented in Python are deployed by the user. These
requirements are listed in the /usr/lib/syslog-ng/python/requirements.txt
file.

Python virtualenv support for development use: syslog-ng is now capable of
using a build-time virtualenv, where all Python development tools are
automatically deployed by the build system.  You can control if you want to
use this using the --with-python-packages configure parameter, the `venv`
value denoting that you want to use the virtualenv, the `system` value
meaning that you want to rely on system packages (as before).  Please note
that syslog-ng has acquired quite a number of these development time
dependencies with the growing number of functionality the Python binding
offers, so using the `system` setting is considered advanced usage, meant to
be used for distro packaging.

Python `LogSource` & `LogFetcher`: a potential deadlock was fixed in
acknowledgement tracking.

Python `LogTemplate`: the use of template functions in templates
instantiated from Python caused a crash, which has been fixed.

Python `Logger`: the low-level Logger class exported by syslog-ng was
wrapped by a logging.LogHandler class so that normal Python APIs for logging
can now be used.
