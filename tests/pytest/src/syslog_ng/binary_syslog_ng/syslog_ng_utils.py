import pytest
from src.syslog_ng.binary_syslog_ng_ctl import syslog_ng_ctl_utils


class SyslogNgUtils(object):
    def __init__(self):
        self.starting_up_message = "syslog-ng starting up"
        self.shutting_down_message = "syslog-ng shutting down"
        self.reloading_message = "Configuration reload request received, reloading configuration;"
        self.global_config = None
        self.file_based_processor = None
        self.config = None
        self.syslog_ng = None

    def set_global_config(self, global_config):
        self.global_config = global_config
        self.file_based_processor = self.global_config['file_based_processor']
        self.config = self.global_config['config']
        self.syslog_ng = self.global_config['syslog_ng']

    def wait_for_starting_up(self):
        internal_log = self.config.get_internal_log_output_path()

        if not self.file_based_processor.wait_for_file_creation(file_path=internal_log):
            self.dump_syslog_ng_console_log()
            self.dump_syslog_ng_internal_log()
            pytest.skip()

        self.file_based_processor.wait_for_expected_message_in_file(file_path=internal_log, expected_message=self.starting_up_message)

        if not syslog_ng_ctl_utils.wait_for_starting_control_socket():
            self.dump_syslog_ng_console_log()
            self.dump_syslog_ng_internal_log()
            pytest.skip()

    def wait_for_shutting_down(self):
        internal_log = self.config.get_internal_log_output_path()

        self.file_based_processor.wait_for_expected_message_in_file(file_path=internal_log, expected_message=self.shutting_down_message)
        syslog_ng_ctl_utils.wait_for_stopping_control_socket()

    def wait_for_reloading(self):
        internal_log = self.config.get_internal_log_output_path()

        self.file_based_processor.wait_for_expected_message_in_file(file_path=internal_log, expected_message=self.reloading_message)

    def dump_syslog_ng_console_log(self):
        console_log = self.syslog_ng.get_syslog_ng_console_log()
        self.file_based_processor.dump_file_content(file_path=console_log)

    def dump_syslog_ng_internal_log(self):
        internal_log = self.config.get_internal_log_output_path()
        self.file_based_processor.dump_file_content(file_path=internal_log)

    def dump_syslog_ng_config(self):
        config_file = self.syslog_ng.get_syslog_ng_config_path()
        self.file_based_processor.dump_file_content(config_file)

    # def check_opened_fds(self, syslog_ng_process_pid):
    #     cmd = "ls -la /proc/%d/fd" % syslog_ng_process_pid
    #     logging.info("command --> [%s]" % cmd)
    #     actual_fd_stat = subprocess.check_output(cmd, stderr=subprocess.PIPE, shell=True)
    #     logging.info("Actual fd stat for syslog-ng process: %s" % actual_fd_stat)
    #     return actual_fd_stat
