from __future__ import print_function, absolute_import
from .completer import Completer


class ChoiceCompleter(Completer):
    def __init__(self, choices, prefix=None, suffix=None):
        self._choices = choices
        self._prefix = prefix or ''
        self._suffix = suffix if suffix is not None else ' '

    def complete(self, entire_input, word_to_be_completed):
        if entire_input.startswith(self._prefix):
            return self._handle_input_with_prefix(entire_input, word_to_be_completed)
        else:
            return self._handle_input_without_prefix(entire_input)

    def _handle_input_without_prefix(self, entire_input):
        if self._prefix.startswith(entire_input) or entire_input == '':
            return [self._prefix]
        else:
            return []

    def _handle_input_with_prefix(self, entire_input, word_to_be_completed):
        entire_input, word_to_be_completed = self._chop_prefixes(entire_input, word_to_be_completed)

        return sorted([self._prefix + choice + self._suffix
                       for choice in self._choices
                       if len(word_to_be_completed) == 0 or choice.startswith(word_to_be_completed)])

    def _chop_prefixes(self, entire_input, word_to_be_completed):
        if word_to_be_completed == entire_input:
            word_to_be_completed = word_to_be_completed[len(self._prefix):]
        entire_input = entire_input[len(self._prefix):]
        return entire_input, word_to_be_completed
