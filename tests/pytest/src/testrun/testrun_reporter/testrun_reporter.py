import logging
from datadiff.tools import assert_equal

class TestrunReporter(object):
    def __init__(self):
        self.input_message_collector = {}
        self.expected_message_collector = {}
        self.actual_message_collector = {}
        self.global_config = None

    def set_global_config(self, global_config):
        self.global_config = global_config

    def save_input_messages(self, source_group_name, input_messages):
        self.input_message_collector[source_group_name] = input_messages

    def save_expected_output_messages(self, destination_group_name, expected_output_messages):
        self.expected_message_collector[destination_group_name] = expected_output_messages

    def save_actual_output_messages(self, destination_group_name, actual_output_messages):
        self.actual_message_collector[destination_group_name] = actual_output_messages

    def compare_expected_and_actual_output_messages(self):
        if self.expected_message_collector != self.actual_message_collector:
            self.dump_testrun_information_on_failed()
        assert_equal(self.expected_message_collector, self.actual_message_collector)
        logging.info("Expected and actual messages are equals.")
        self.dump_testrun_messages()

    def dump_testrun_messages(self):
        logging.info("Input messages: %s" % str(self.input_message_collector))
        logging.info("Expected output messages: %s" % str(self.expected_message_collector))
        logging.info("Actual messages: %s" % str(self.actual_message_collector))

    def dump_testrun_information_on_failed(self):
        # syslog_ng_control_tool = SyslogNgCtl()
        # syslog_ng_control_tool.dump_syslog_ng_stats()
        self.global_config['syslog_ng_utils'].dump_syslog_ng_console_log()
        self.global_config['syslog_ng_utils'].dump_syslog_ng_internal_log()
        self.dump_testrun_messages()
