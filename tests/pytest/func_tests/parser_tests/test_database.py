from func_tests import generate_tests_for_options
from func_tests.parser_tests.setup_parser import *

option_place_in_config = "parser_options"

def pytest_generate_tests(metafunc):
    option_name = "database"
    generate_tests_for_options.generate_tests_for_option(metafunc, option_name, option_place_in_config)

def test_bad_hostname(setup_for_parser_options, option_name, option_place, tested_option_value, tested_product_version):
    print("AAAAAAAA3")
    print("option name: %s " % option_name)
    print("option place: %s " % option_place)
    print("tested value: %s" % tested_option_value)
    print("tested version: %s" % tested_product_version)
    # test_reporter = TestReporter()
    # sender = Sender()
    # receiver = Receiver()
    #
    #
    # if tested_option_value == "default":
    #     sender.send_default_messages_to_source_side(test_reporter)
    #     receiver.collect_actual_output_messages_from_destination_side(test_reporter)
    #
    # if tested_option_value == "string":
    #     sender.send_default_messages_to_source_side(test_reporter)
    #     receiver.collect_actual_output_messages_from_destination_side(test_reporter)
    #
    # test_reporter.compare_expected_and_actual_output_messages()
