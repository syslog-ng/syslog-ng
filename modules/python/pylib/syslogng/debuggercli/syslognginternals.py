#
# This module contains a stub for all functions exported by syslog-ng, so when
# the module is imported from within syslog-ng it uses the exported functions,
# in case we are running outside of syslog-ng we are using the stubs that is
# enough for testing.
#
from __future__ import print_function, absolute_import


def get_nv_registry():
    return ('0', '1', '2', '3', '4', "DATE", "HOST", "MSGHDR", "PROGRAM", "PID", "MSG", '.unix.uid', '.unix.gid')


# override implementations from the module supplied by the C implementation.
try:
    from _syslogngdbg import *
except ImportError:
    pass
