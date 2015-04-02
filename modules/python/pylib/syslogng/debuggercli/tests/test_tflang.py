from .test_completerlang import CompleterLangTestCase
from ..tflang import TemplateFunctionLang


class TestTFLang(CompleterLangTestCase):
    def setUp(self):
        self._parser = TemplateFunctionLang()

    def test_template_func_is_a_command_and_a_sequence_of_arguments(self):
        self._assert_token_follows("", ["COMMAND"])
        self._assert_token_follows("foo ", ["ARG"])
        self._assert_token_follows("foo bar ", ["ARG"])
        self._assert_token_follows("foo bar baz", ["ARG"])

    def test_format_json_arguments(self):
        self._assert_token_follows("format-json", ["COMMAND_FORMAT_JSON"])
        self._assert_token_follows("format-json ", ["ARG", "OPT__KEY", "OPT__PAIR"])
        self._assert_token_follows("format-json --key ", ["name_value_name"])
        self._assert_token_follows("format-json --pair ", ["name_value_pair"])
        self._assert_token_follows("format-json --scope ", ["value_pairs_scope"])
        self._assert_token_follows("format-json --key * --pair a=$b --scope ", ["value_pairs_scope"])

    def test_geoip_arguments(self):
        self._assert_token_follows("geoip ", ["ipaddress", "ARG"])

    def test_echo_arguments(self):
        self._assert_token_follows("echo ", ["template", "ARG"])
        self._assert_token_follows("echo $MSG ", ["template", "ARG"])
        self._assert_token_follows("echo $MSG $MSG ", ["template", "ARG"])
