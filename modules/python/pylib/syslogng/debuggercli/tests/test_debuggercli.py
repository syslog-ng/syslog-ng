from __future__ import absolute_import, print_function
from .test_completer import CompleterTestCase
from ..debuggercli import DebuggerCLI


class TestDebuggerCLI(CompleterTestCase):
    def _construct_completer(self):
        return DebuggerCLI().get_root_completer()

    def test_no_text_offers_the_potential_prefixes_of_a_template_expr(self):
        self._assert_completions_offered(entire_input="print ", word="", expected_completions=['$', '$(', '${'])

    def test_literal_text_offers_potential_prefixes_of_a_template_expr(self):
        self._assert_completions_offered(entire_input="print 'foobar ", word="", expected_completions=['$', '$(', '${'])

    def test_dollar_offers_possible_macro_completions(self):
        self._assert_completions_offered(entire_input="print $", word="$",
                                         expected_completions=['$MSG', '$MSGHDR', '$DATE', '${', '$('])

    def test_dollar_brace_offers_possible_macro_completions(self):
        self._assert_completions_offered(entire_input="print ${", word="${",
                                         expected_completions=["${MSG}", "${DATE}"])

    def test_dollar_paren_offers_template_functions(self):
        self._assert_completions_offered(entire_input="print $(", word="$(",
                                         expected_completions=["$(echo "])

    def test_literal_text_plus_dollar_offers_macros_using_the_literal_as_prefix(self):
        self._assert_completions_offered(entire_input="print foobar$", word="foobar$",
                                         expected_completions=["foobar$("])

    def test_literal_text_plus_dollar_paren_offers_template_functions_using_the_literal_as_prefix(self):
        self._assert_completions_offered(entire_input="print foobar$(", word="foobar$(",
                                         expected_completions=["foobar$(echo ", 'foobar$(format-json '])

    def test_literal_text_plus_dollar_brace_offers_macros_using_the_literal_as_prefix(self):
        self._assert_completions_offered(entire_input="print foobar${", word="foobar${",
                                         expected_completions=["foobar${MSG}"])

    def test_template_function_offers_potential_opts(self):
        self._assert_completions_offered(entire_input="print 'foobar$(format-json ", word="",
                                         expected_completions=['--pair '])
        self._assert_completions_offered(entire_input="print 'foobar$(echo ", word="",
                                         expected_completions=['$', '$(', '${'])

    def test_complex_cases(self):
        self._assert_completions_offered(entire_input="print 'foo$(format-json --key *)bar", word="",
                                         expected_completions=['$(', '${', '$'])
        self._assert_completions_offered(entire_input="print 'foo$(format-json --key *)bar$(echo ", word="",
                                         expected_completions=['$(', '${', '$'])
        self._assert_completions_offered(entire_input="print 'foo$(format-json --key *)${MSG}$(echo ", word="",
                                         expected_completions=['$(', '${', '$'])
        self._assert_completions_offered(entire_input="print 'foo$(format-json --key *)${MSG}$(echo $MSGHDR)", word="",
                                         expected_completions=['$(', '${', '$'])
