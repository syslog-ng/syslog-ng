from __future__ import absolute_import, print_function
import unittest
from ..debuglang import DebugLang


class CompleterLangTestCase(unittest.TestCase):
    def _get_expected_tokens(self, text):
        drop_last_token = not text[-1:].isspace()
        return self._parser.get_expected_tokens(text, drop_last_token=drop_last_token)

    def _assert_token_follows(self, text, tokens, pos=None, replaced_token=None):
        (expected_tokens, rtoken, rtoken_pos) = self._get_expected_tokens(text)
        if pos is not None:
            self.assertEquals(pos, rtoken_pos)
        if replaced_token is not None:
            self.assertEquals(rtoken.value if rtoken is not None else '', replaced_token)
        self.assertLessEqual(set(tokens), set(expected_tokens))


class TestCompleterLang(CompleterLangTestCase):
    """This testcase intends to test the CompleterLang base class by
    an example grammar, which happens to be DebugLang. The DebugLang grammar
    itself is tested in a separate testcase."""

    def setUp(self):
        self._parser = DebugLang()

    def test_all_tokens_are_found_that_would_match_rules_in_the_grammar(self):
        self._assert_command_token_follows("", pos=0, replaced_token="")
        self._assert_command_token_follows(" ", pos=1, replaced_token="")
        self._assert_command_token_follows("foo", pos=0, replaced_token="foo")

        self._assert_token_follows("foo ", ["ARG"], pos=4, replaced_token="")
        self._assert_token_follows("foo bar", ["ARG"], pos=4, replaced_token="bar")
        self._assert_token_follows("foo bar ", ["ARG"], pos=8, replaced_token="")
        self._assert_token_follows("print ", ["template", "ARG"], pos=6, replaced_token="")
        self._assert_token_follows("print $MSG", ["template", "ARG"], pos=6, replaced_token="$MSG")
        self._assert_token_follows("print $MSG ", [], pos=11, replaced_token="")

    def _get_command_tokens(self):
        return [token for token in self._parser.tokens if token.startswith("COMMAND")]

    def _assert_command_token_follows(self, text, pos=None, replaced_token=None):
        self._assert_token_follows(text, set(self._get_command_tokens()), pos, replaced_token)
