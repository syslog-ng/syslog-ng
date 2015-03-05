from __future__ import absolute_import, print_function
from ..templatelang import TemplateLang
from .test_completerlang import CompleterLangTestCase


class TestTemplateLang(CompleterLangTestCase):
    def setUp(self):
        self._parser = TemplateLang()

    def test_template_is_a_series_of_literal_macro_and_template_funcs(self):
        self._assert_token_follows("", ["LITERAL", "MACRO", "TEMPLATE_FUNC"])
        self._assert_token_follows(" ", ["LITERAL", "MACRO", "TEMPLATE_FUNC"])
        self._assert_token_follows("foo ", ["LITERAL", "MACRO", "TEMPLATE_FUNC"])
        self._assert_token_follows("$MSG ", ["LITERAL", "MACRO", "TEMPLATE_FUNC"])
        self._assert_token_follows("${MSG} ", ["LITERAL", "MACRO", "TEMPLATE_FUNC"])
        self._assert_token_follows("$(echo $MSG) ", ["LITERAL", "MACRO", "TEMPLATE_FUNC"])
        self._assert_token_follows("foo$MSG${MSG}$(echo $MSG)bar ", ["LITERAL", "MACRO", "TEMPLATE_FUNC"])
