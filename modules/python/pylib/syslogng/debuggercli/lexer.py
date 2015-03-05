from __future__ import print_function, absolute_import
from abc import ABCMeta, abstractmethod


class Lexer(object):
    __metaclass__ = ABCMeta

    @abstractmethod
    def token(self):
        raise NotImplementedError

    @abstractmethod
    def input(self, input):
        raise NotImplementedError

    @abstractmethod
    def get_position(self):
        raise NotImplementedError
