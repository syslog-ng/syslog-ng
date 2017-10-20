import pytest
from source.testdb.initializer.setup_classes import SetupClasses

debugloglevel = pytest.mark.skipif(
    pytest.config.getoption("--loglevel") != "debug",
    reason="need --loglevel=debug option to run"
)


def init_unittest(request):
    global sc, test_message, logsource, log_writer
    sc = SetupClasses(request)
    test_message = "Test message"
    logsource = "FromUnitTest"
    log_writer = sc.testdb_logger.set_logger(logsource=logsource)


def check_testcase_report_file_content(expected_loglevel):
    with open(sc.testdb_path_database.testcase_report_file, 'r') as file_object:
        report_file_content = file_object.read()
        assert " - %s - %s - %s" % (logsource, expected_loglevel, test_message) in report_file_content


def test_info_level_in_file_content(request):
    init_unittest(request)
    log_writer.info(test_message)
    check_testcase_report_file_content("INFO")


@debugloglevel
def test_debug_level_in_file_content(request):
    init_unittest(request)
    log_writer.debug(test_message)
    check_testcase_report_file_content("DEBUG")


def test_error_level_in_file_content(request):
    init_unittest(request)
    log_writer.error(test_message)
    check_testcase_report_file_content("ERROR")


def test_write_message_based_on_value_negative(request):
    init_unittest(request)
    sc.testdb_logger.write_message_based_on_value(logsource=log_writer, message=test_message, value=False, positive_loglevel="info")
    check_testcase_report_file_content("ERROR")


def test_write_message_based_on_value_positive(request):
    init_unittest(request)
    sc.testdb_logger.write_message_based_on_value(logsource=log_writer, message=test_message, value=True, positive_loglevel="info")
    check_testcase_report_file_content("INFO")
