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

from .test_completer import CompleterTestCase
from ..macrocompleter import MacroCompleter


class TestMacroCompleter(CompleterTestCase):
    # pylint: disable=arguments-differ
    def _construct_completer(self, macros=None):
        return MacroCompleter(macros=macros or ['MSG', 'MSGHDR', 'HOST',
                                                '.unix.uid', '.unix.gid',
                                                'S_DATE', 'R_MONTH', 'C_WEEK', 'DATE',
                                                'smallcaps',
                                                '0', '1', '2', '10', '100'])

    def test_only_a_dollar_is_offered_for_an_empty_string(self):
        self._assert_completions_offered('', ('$', '${'))

    def test_dollar_brace_is_offered_for_a_dollar(self):
        self._assert_completion_offered('$', '${')

    def test_unqualified_macros_are_offered_for_a_dollar(self):
        self._assert_completions_offered('$', ('$MSG', '$HOST'))

    def test_qualified_macros_are_offered_with_a_brace_for_a_dollar(self):
        self._assert_completion_offered('$', '${.unix.uid}')
        self._assert_completion_not_offered('$', '$.unix.uid')

    def test_small_numbered_matches_are_offered_for_a_dollar(self):
        # small means < 10
        self._assert_completion_offered('$', '$0')

    def test_large_numbered_matches_are_not_offered_for_a_dollar(self):
        # large means > 10
        self._assert_completions_not_offered('$', ('$10', '$100'))

    def test_all_numbered_matches_are_offered_for_dollar_plus_digit(self):
        # ${1 ... lists all numbered macros
        self._assert_completions_offered('$1', ('$1', '$10', '$100'))

    def test_all_macros_except_large_numbered_matches_are_offered_for_a_dollar_brace(self):
        # ${ }
        self._assert_completions_offered('${', ('${HOST}', '${MSG}', '${.unix.uid}', '${0}', '${1}'))
        self._assert_completions_not_offered('${', ('${10}', '${100}'))

    def test_all_numbered_matches_are_offered_for_dollar_brace_digit(self):
        self._assert_completions_offered('${1', ('${1}', '${10}', '${100}'))

    def test_date_macros_are_not_offered_for_a_dollar(self):
        self._assert_completions_not_offered('$', ('$S_DATE', '$R_MONTH'))

    def test_date_shortcuts_are_offered_for_a_dollar(self):
        self._assert_completions_offered('$', ['$R_* (received time)',
                                               '$S_* (stamp in message)',
                                               '$C_* (current time)'])

    def test_date_shortcuts_are_offered_for_a_dollar_brace(self):
        self._assert_completions_offered('${', ['${R_*} (received time)',
                                                '${S_*} (stamp in message)',
                                                '${C_*} (current time)'])

    def test_date_macros_are_offered_if_the_prefix_is_correct(self):
        self._assert_completions_offered('$R_', ['$R_MONTH'])
        self._assert_completions_offered('${R_', ['${R_MONTH}'])
        self._assert_completions_offered('$S_', ['$S_DATE'])
        self._assert_completions_offered('${S_', ['${S_DATE}'])
        self._assert_completions_offered('$C_', ['$C_WEEK'])
        self._assert_completions_offered('${C_', ['${C_WEEK}'])

    def test_date_macros_are_not_offered_if_the_prefix_doesnot_match(self):
        self._assert_completion_not_offered('$R', '$R_MONTH')
        self._assert_completion_not_offered('$S', '$S_DATE')
        self._assert_completion_not_offered('$C', '$C_DATE')

    def test_date_wildcards_are_not_offered_if_the_prefix_is_correct(self):
        self._assert_completions_not_offered('$R_', '$R_* (received time)')
        self._assert_completions_not_offered('${R_', '${R_*} (received time)')
        self._assert_completions_not_offered('$S_', '$S_* (stamp in message)')
        self._assert_completions_not_offered('${S_}', '${S_*} (stamp in message)')
        self._assert_completions_not_offered('$C_', '$C_* (current time)')
        self._assert_completions_not_offered('${C_', '${C_*} (current time)')

    def test_prefix_is_as_good_as_the(self):
        # $ + number as prefix
        self._assert_completions_offered('$10', ('$10', '$100'))
        # $ + macro first character
        self._assert_completions_offered('$M', ('$MSGHDR', '$MSG'))
        # $ + macro first character (small caps)
        self._assert_completions_offered('$s', ('$smallcaps',))
        # $ + brace + number
        self._assert_completions_offered('${10', ('${10}', '${100}'))
        # $ + brace + unqualified macro first character
        self._assert_completions_offered('${M', ('${MSGHDR}', '${MSG}'))
        # $ + brace + qualified macro first character
        self._assert_completions_offered('${.', ('${.unix.uid}', '${.unix.gid}'))

    def test_only_completions_that_start_with_word_are_listed_as_completions(self):
        for word in ('$', '$1', '$M'):
            self._assert_completions_start_with_word(word)

    def test_nothing_is_offered_if_the_entire_input_diverged_already(self):
        completions = self._get_completions('', entire_input='$(echo ')
        self.assertEqual(completions, [])
        completions = self._get_completions('', entire_input='almafa ')
        self.assertEqual(completions, [])
