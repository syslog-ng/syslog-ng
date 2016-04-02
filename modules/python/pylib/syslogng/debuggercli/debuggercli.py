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
from .langcompleter import LangBasedCompleter
from .debuglang import DebugLang
from .templatelang import TemplateLang
from .tflang import TemplateFunctionLang
from .choicecompleter import ChoiceCompleter
from .macrocompleter import MacroCompleter
from .syslognginternals import get_debugger_commands, get_template_functions, get_nv_registry, get_value_pairs_scopes


class DebuggerCLI(object):
    debug_completers = {}
    template_completers = {}
    tflang_completers = {}

    def _setup_tflang_completers(self):
        self.tflang_completers['COMMAND'] = ChoiceCompleter(get_template_functions())
        self.tflang_completers['name_value_name'] = ChoiceCompleter(get_nv_registry())
        self.tflang_completers['value_pairs_scope'] = ChoiceCompleter(get_value_pairs_scopes())
        self.tflang_completers['OPT'] = ChoiceCompleter(TemplateFunctionLang.known_options)
        self.tflang_completers['template'] = LangBasedCompleter(parser=TemplateLang(),
                                                                completers=self.template_completers)

    def _setup_template_completers(self):
        self.template_completers['MACRO'] = MacroCompleter()
        self.template_completers['TEMPLATE_FUNC'] = LangBasedCompleter(
            parser=TemplateFunctionLang(),
            completers=self.tflang_completers,
            prefix="$(")

    def _setup_debug_completers(self):
        self.debug_completers['COMMAND'] = ChoiceCompleter(get_debugger_commands())
        self.debug_completers['template'] = LangBasedCompleter(parser=TemplateLang(),
                                                               completers=self.template_completers)

    def get_root_completer(self):
        self._setup_debug_completers()
        self._setup_template_completers()
        self._setup_tflang_completers()
        return LangBasedCompleter(parser=DebugLang(),
                                  completers=self.debug_completers)
