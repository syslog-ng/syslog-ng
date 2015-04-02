from __future__ import absolute_import
from syslogng.debuggercli.readline import setup_readline


def fetch_command():
    setup_readline()
    return raw_input("(syslog-ng) ")
