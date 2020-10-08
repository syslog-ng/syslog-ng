#############################################################################
# Copyright (c) 2020 One Identity
# Copyright (c) 2020 Laszlo Budai <laszlo.budai@outlook.com>
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
from .debuggercli import DebuggerCLI

from editline import _editline
from editline.editline import EditLine
from editline import lineeditor

import sys

class EditlineCompleteHook(object):
    def __init__(self, completer, editl):
        self._completer = completer
        self._last_contents = (None, None)
        self._last_completions = []
        self.editl = editl

    def complete(self, text):
        return self._get_completions(self.editl.get_line_buffer(), text)

    def _get_completions(self, entire_text, text):
        if self._last_contents == (entire_text, text):
            return self._last_completions
        self._last_completions = self._completer.complete(entire_text, text)
        self._last_contents = (entire_text, text)
        return self._last_completions

class MyEditLineCompleter(lineeditor.Completer):
    def __init__(self, subeditor, completer, namespace = None):
        super().__init__(subeditor, namespace)
        self._default_display_matches = self.subeditor.display_matches
        self.subeditor.display_matches = self.display_matches
        self.completer = completer 
        self.subeditor.completer = self.complete

    def complete(self, text):
        self.matches = self.completer.complete(text)
        return self.matches

    def display_matches(self, matches):
        self.subeditor._display_matches(self.matches)



__setup_performed__ = False


def setup_editline():
    # pylint: disable=global-statement
    global __setup_performed__

    if __setup_performed__:
        return

    debuggercli = DebuggerCLI()

    editline_system = _editline.get_global_instance()
    if editline_system is None:
      sys_el = EditLine("DebuggerCLI", sys.stdin, sys.stdout, sys.stderr)
      _editline.set_global_instance(sys_el)
      completer = EditlineCompleteHook(debuggercli.get_root_completer(), sys_el)
      sys_line_ed = MyEditLineCompleter(sys_el, completer=completer)
      lineeditor.global_line_editor(sys_line_ed)
      sys_el.completer = sys_line_ed.complete

    __setup_performed__ = True
