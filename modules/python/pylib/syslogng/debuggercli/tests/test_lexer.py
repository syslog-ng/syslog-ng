import unittest


class TestLexer(unittest.TestCase):

    def setUp(self):
        self._lexer = self._construct_lexer()
        self._current_token = None

    def _construct_lexer(self, *args, **kwargs):
        raise NotImplementedError

    def _next_token(self):
        self._current_token = self._lexer.token()
        return self._current_token

    def _assert_current_token_type_equals(self, token_type):
        self.assertEquals(self._current_token.type, token_type)

    def _assert_current_token_value_equals(self, value):
        self.assertEquals(self._current_token.value, value)

    def _assert_current_token_pos_equals(self, pos):
        self.assertEquals(self._current_token.lexpos, pos)

    def _assert_current_token_is_partial(self):
        self.assertTrue(self._current_token.partial)

    def _assert_current_token_is_not_partial(self):
        self.assertFalse(self._current_token.partial)

    def _assert_current_token_equals(self, token_type=None, value=None, pos=None, partial=None):
        if token_type is not None:
            self._assert_current_token_type_equals(token_type)
        if value is not None:
            self._assert_current_token_value_equals(value)
        if pos is not None:
            self._assert_current_token_pos_equals(pos)
        if partial is not None:
            if partial:
                self._assert_current_token_is_partial()
            else:
                self._assert_current_token_is_not_partial()

    def _assert_next_token_equals(self, *args, **kwargs):
        self._next_token()
        self._assert_current_token_equals(*args, **kwargs)

    def _assert_next_token_type_equals(self, token_type):
        self._next_token()
        self._assert_current_token_type_equals(token_type)

    def _assert_next_token_is_partial(self):
        self._next_token()
        self._assert_current_token_is_partial()

    def _assert_next_token_is_none(self):
        self.assertIsNone(self._next_token())

    def _assert_next_token_value_equals(self, value):
        self._next_token()
        self._assert_current_token_value_equals(value)
