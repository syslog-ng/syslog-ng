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

from __future__ import absolute_import

def is_readline_available():
    try:
        import readline
    except ImportError:
        return False
    return True

def is_editline_available():
    try:
        import editline
    except ImportError:
        return False
    return True

def setup_read_or_editline():
    if is_readline_available():
        from syslogng.debuggercli.readline import setup_readline
        setup_readline()
    elif is_editline_available():
        from syslogng.debuggercli.editline import setup_editline
        setup_editline()

def fetch_command():
    setup_read_or_editline()
    try:
        input_function = raw_input
    except NameError:
        input_function = input
    return input_function("(syslog-ng) ")
