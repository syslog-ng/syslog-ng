import sys

from src.common.waiters import wait_till_statement_not_true, wait_till_statement_not_false
from src.drivers.file.file_reader import FileReader
from src.syslog_ng.configuration.config import SyslogNgConfig
from src.syslog_ng.syslog_ng_ctl.syslog_ng_ctl import SyslogNgCtl
from src.syslog_ng.syslog_ng_path.path import SyslogNgPath


class SyslogNgUtils(object):
    def __init__(self):
        self.syslog_ng_ctl = SyslogNgCtl()
        self.file_reader = FileReader()
        self.config_object = SyslogNgConfig.Instance()
        self.syslog_ng_path = SyslogNgPath()
        self.starting_up_message = "syslog-ng starting up"
        self.shutting_down_message = "syslog-ng shutting down"
        self.reloading_message = "Configuration reload request received, reloading configuration;"

    def wait_for_starting_process(self):
        if self.file_reader.wait_for_expected_message(file_path=self.config_object.internal_log_file_path,
                                                      expected_message=self.starting_up_message) and self.wait_for_starting_control_socket():
            return True
        else:
            self.dump_syslog_ng_consol_log()
            sys.exit(1)

    def wait_for_stopping_process(self):
        if self.file_reader.wait_for_expected_message(file_path=self.config_object.internal_log_file_path,
                                                      expected_message=self.shutting_down_message) and self.wait_for_stopping_control_socket():
            return True
        else:
            self.dump_syslog_ng_consol_log()
            sys.exit(1)

    def wait_for_reloading_process(self):
        if self.file_reader.wait_for_expected_message(file_path=self.config_object.internal_log_file_path,
                                                      expected_message=self.reloading_message):
            return True
        else:
            self.dump_syslog_ng_consol_log()
            sys.exit(1)

    def wait_for_starting_control_socket(self):
        return wait_till_statement_not_true(self.syslog_ng_ctl.is_syslog_ng_running)

    def wait_for_stopping_control_socket(self):
        return wait_till_statement_not_false(self.syslog_ng_ctl.is_syslog_ng_running)

    def is_stat_in_stats(self, stat):
        return stat in str(self.syslog_ng_ctl.stats())

    def wait_for_statistics_count(self, stat):
        return wait_till_statement_not_true(self.is_stat_in_stats, stat)

    def dump_syslog_ng_consol_log(self):
        self.file_reader.dump_file_content(file_path=self.syslog_ng_path.get_syslog_ng_console_log())
        self.file_reader.dump_file_content(file_path=self.config_object.internal_log_file_path)
        self.file_reader.dump_file_content(file_path=self.config_object.config_path)
