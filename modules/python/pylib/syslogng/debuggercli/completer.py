from __future__ import print_function, absolute_import
from abc import abstractmethod, ABCMeta


class Completer(object):
    __metaclass__ = ABCMeta

    @abstractmethod
    def complete(self, entire_text, word_to_be_completed):
        """Function to return the list of potential completion for a word"""
        raise NotImplementedError
