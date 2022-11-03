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

from __future__ import absolute_import, print_function
import readline
import traceback
from .debuggercli import DebuggerCLI


class ReadlineCompleteHook(object):
    def __init__(self, completer):
        self._completer = completer
        self._last_contents = (None, None)
        self._last_completions = []

    def complete(self, text, state):
        # pylint: disable=broad-except
        try:
            entire_text = readline.get_line_buffer()[:readline.get_endidx()]
            completions = self._get_completions(entire_text, text)
            return completions[state]
        except Exception:
            traceback.print_exc()

    def _get_completions(self, entire_text, text):
        if self._last_contents == (entire_text, text):
            return self._last_completions
        self._last_completions = self._completer.complete(entire_text, text)
        self._last_completions.append(None)
        self._last_contents = (entire_text, text)
        return self._last_completions


__setup_performed__ = False


def setup_readline():
    # pylint: disable=global-statement
    global __setup_performed__

    if __setup_performed__:
        return

    debuggercli = DebuggerCLI()
    readline.parse_and_bind("tab: complete")
    readline.set_completer(ReadlineCompleteHook(debuggercli.get_root_completer()).complete)
    readline.set_completer_delims(' \t\n\"\'`@><=;|&')
    __setup_performed__ = True
