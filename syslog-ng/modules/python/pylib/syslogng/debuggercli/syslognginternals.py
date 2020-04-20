#############################################################################
# Copyright (c) 2015-2016 Balabit
#
# This program is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License version 2 as published
# by the Free Software Foundation, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
#
# As an additional exemption you are allowed to compile & link against the
# OpenSSL libraries as published by the OpenSSL project. See the file
# COPYING for details.
#
#############################################################################
#
# This module contains a stub for all functions exported by syslog-ng, so when
# the module is imported from within syslog-ng it uses the exported functions,
# in case we are running outside of syslog-ng we are using the stubs that is
# enough for testing.
#
from __future__ import print_function, absolute_import


def get_nv_registry():
    return ('0', '1', '2', '3', '4', "DATE", "HOST", "MSGHDR", "PROGRAM", "PID", "MSG", '.unix.uid', '.unix.gid')


def get_debugger_commands():
    return (
        "help",
        "continue",
        "print",
        "display",
        "drop",
        "quit"
    )


def get_template_functions():
    return (
        "python",
        "graphite-output",
        "uuid",
        "hash",
        "sha1",
        "sha256",
        "sha512",
        "md4",
        "md5",
        "format-json",
        "grep",
        "if",
        "or",
        "echo",
        "length",
        "substr",
        "strip",
        "sanitize",
        "lowercase",
        "uppercase",
        "replace-delimiter",
        "padding",
        "+",
        "-",
        "*",
        "/",
        "%",
        "ipv4-to-int",
        "indent-multi-line",
        "context-length",
        "env",
        "template"
    )


def get_value_pairs_scopes():
    return (
        "nv-pairs",
        "dot-nv-pairs",
        "all-nv-pairs",
        "rfc3164",
        "core",
        "base",
        "rfc5424",
        "syslog-proto",
        "all-macros",
        "selected-macros",
        "sdata",
        "everything",
    )


# override implementations from the module supplied by the C implementation.
try:
    # pylint: disable=import-error,wildcard-import,wrong-import-position
    from _syslogngdbg import *
except ImportError:
    pass
