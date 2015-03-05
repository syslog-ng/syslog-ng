from __future__ import absolute_import, print_function
from ..debuglang import DebugLang
from .test_completerlang import CompleterLangTestCase


class TestDebugLang(CompleterLangTestCase):
    def setUp(self):
        self._parser = DebugLang()

    def test_first_token_is_a_command(self):
        self._assert_command_token_follows("")
        self._assert_command_token_follows(" ")
        self._assert_command_token_follows("foo")

    def test_print_expects_a_template_arg(self):
        self._assert_token_follows("print ", ["template", "ARG"])
        self._assert_token_follows("print $MSG ", [])

    def test_display_expects_a_template_arg(self):
        self._assert_token_follows("display ", ["template", "ARG"])
        self._assert_token_follows("display $MSG ", [])

    def test_generic_command_expects_a_series_of_args(self):
        self._assert_token_follows("foo ", ["ARG"])
        self._assert_token_follows("foo arg ", ["ARG"])
        self._assert_token_follows("foo arg arg ", ["ARG"])

    def _get_command_tokens(self):
        return [token for token in self._parser.tokens if token.startswith("COMMAND")]

    def _assert_command_token_follows(self, text, pos=None, replaced_token=None):
        self._assert_token_follows(text, set(self._get_command_tokens()), pos, replaced_token)
