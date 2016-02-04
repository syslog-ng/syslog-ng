from __future__ import absolute_import, print_function
import readline
import traceback
from .debuggercli import DebuggerCLI


class ReadlineCompleteHook(object):
    def __init__(self, completer):
        self._completer = completer
        self._last_contents = (None, None)
        self._last_completions = []

    def complete(self, text, state):
        # pylint: disable=broad-except
        try:
            entire_text = readline.get_line_buffer()[:readline.get_endidx()]
            completions = self._get_completions(entire_text, text)
            return completions[state]
        except Exception:
            traceback.print_exc()

    def _get_completions(self, entire_text, text):
        if self._last_contents == (entire_text, text):
            return self._last_completions
        self._last_completions = self._completer.complete(entire_text, text)
        self._last_completions.append(None)
        self._last_contents = (entire_text, text)
        return self._last_completions


__setup_performed__ = False


def setup_readline():
    # pylint: disable=global-statement
    global __setup_performed__

    if __setup_performed__:
        return

    debuggercli = DebuggerCLI()
    readline.parse_and_bind("tab: complete")
    readline.set_completer(ReadlineCompleteHook(debuggercli.get_root_completer()).complete)
    readline.set_completer_delims(' \t\n\"\'`@><=;|&')
    __setup_performed__ = True
