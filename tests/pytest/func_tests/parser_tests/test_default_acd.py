from func_tests.testcase import Testcase
from func_tests.parser_tests.setup_parser import *

def pytest_funcarg__preconditions(request):
    preconditions = Testcase()

    preconditions.config.add_global_options({"keep_hostname": "yes"})
    s_file = preconditions.config.add_source_group(driver_name="file")
    database_content = """'mymachine','email','aaa@aaa.com'"""
    p_acd = preconditions.config.add_parser_group(driver_name="add_contextual_data", options={"selector": '@@${HOST}@@'}, database_content=database_content)
    d_file = preconditions.config.add_destination_group(driver_name="file")
    preconditions.config.connect_drivers_in_logpath(sources=[s_file], parsers=[p_acd], destinations=[d_file])

    return preconditions


def test_default_acd(setup_parser_acd, preconditions):
    print("\n-->FROM test")

    # preconditions.sender.send_default_messages_to_source_side(counter=5)

    preconditions.sender.send_custom_messages_to_source_side(priority="22", hostname="mymachine", program="dfdsfd", message="asdass")
    preconditions.receiver.collect_actual_output_messages_from_destination_side(priority="22", hostname="mymachine", program="dfdsfd", message="asdass")
    preconditions.test_reporter.compare_expected_and_actual_output_messages()


