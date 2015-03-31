from __future__ import print_function, absolute_import
import unittest


class CompleterTestCase(unittest.TestCase):
    def setUp(self):
        self._completer = self._construct_completer()

    def _construct_completer(self):
        raise NotImplementedError

    def _get_completions(self, word, entire_input=None):
        return self._completer.complete(entire_input or word, word)

    def _assert_completion_offered(self, word, completion):
        self._assert_completions_offered(word, [completion])

    def _assert_completion_not_offered(self, word, completion):
        self._assert_completions_not_offered(word, [completion])

    def _assert_no_completions_are_offered(self, word, entire_input=None):
        completions = self._get_completions(word, entire_input=entire_input)
        self.assertEquals(completions, [])

    def _assert_completions_offered(self, word, expected_completions, entire_input=None):
        completions = self._get_completions(word, entire_input=entire_input)
        for completion in expected_completions:
            self.assertIn(completion, completions)
        self.assertEqual(completions, sorted(completions))

    def _assert_completions_not_offered(self, word, unexpected_completions, entire_input=None):
        completions = self._get_completions(word, entire_input=entire_input)
        for completion in unexpected_completions:
            self.assertNotIn(completion, completions)

    def _assert_completions_start_with_word(self, word):
        completions = self._get_completions(word)
        for completion in completions:
            self.assertTrue(completion.startswith(word),
                            msg="Completion {} does not start with the word to be completed {}"
                                .format(completion, word))
