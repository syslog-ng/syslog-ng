from __future__ import print_function, absolute_import
from .completerlang import CompleterLang
from .templatelexer import TemplateLexer


class TemplateLang(CompleterLang):

    tokens = [
        "LITERAL", "MACRO", "TEMPLATE_FUNC"
    ]

    @staticmethod
    def p_template_string(p):
        '''template_string : template_elems'''
        pass

    @staticmethod
    def p_template_elems(p):
        '''template_elems : template_elem template_elems
                          |'''
        pass

    @staticmethod
    def p_template_elem(p):
        '''template_elem : LITERAL
                         | MACRO
                         | TEMPLATE_FUNC
        '''
        pass

    def _construct_lexer(self):
        return TemplateLexer()
