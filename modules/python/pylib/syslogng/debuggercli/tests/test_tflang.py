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

from .test_completerlang import CompleterLangTestCase
from ..tflang import TemplateFunctionLang


class TestTFLang(CompleterLangTestCase):
    def setUp(self):
        self._parser = TemplateFunctionLang()

    def test_template_func_is_a_command_and_a_sequence_of_arguments(self):
        self._assert_token_follows("", ["COMMAND"])
        self._assert_token_follows("foo ", ["ARG"])
        self._assert_token_follows("foo bar ", ["ARG"])
        self._assert_token_follows("foo bar baz", ["ARG"])

    def test_format_json_arguments(self):
        self._assert_token_follows("format-json", ["COMMAND_FORMAT_JSON"])
        self._assert_token_follows("format-json ", ["ARG", "OPT__KEY", "OPT__PAIR"])
        self._assert_token_follows("format-json --key ", ["name_value_name"])
        self._assert_token_follows("format-json --pair ", ["name_value_pair"])
        self._assert_token_follows("format-json --scope ", ["value_pairs_scope"])
        self._assert_token_follows("format-json --key * --pair a=$b --scope ", ["value_pairs_scope"])

    def test_echo_arguments(self):
        self._assert_token_follows("echo ", ["template", "ARG"])
        self._assert_token_follows("echo $MSG ", ["template", "ARG"])
        self._assert_token_follows("echo $MSG $MSG ", ["template", "ARG"])
