import logging
import sys

from src.common.run_command import run_command_for_pid, run_command_for_exit_code
from src.syslog_ng.configuration.config import SyslogNgConfig
from src.syslog_ng.syslog_ng_ctl.syslog_ng_ctl import SyslogNgCtl
from src.syslog_ng.syslog_ng_path.path import SyslogNgPath
from src.syslog_ng.syslog_ng_utils.utils import SyslogNgUtils


class SyslogNg(object):
    def __init__(self):
        self.process_pid = None
        self.config_object = SyslogNgConfig.Instance()
        self.syslog_ng_path = SyslogNgPath()
        self.syslog_ng_ctl = SyslogNgCtl()
        self.syslog_ng_utils = SyslogNgUtils()

    def start(self, expected_start=True):
        self.config_object.write_config()
        start_command = "%s " % self.syslog_ng_path.get_syslog_ng_binary()
        start_command += "-Ftv --no-caps --enable-core "
        start_command += "-f %s " % self.syslog_ng_path.get_syslog_ng_config_path()
        start_command += "-R %s " % self.syslog_ng_path.get_syslog_ng_persist()
        start_command += "-p %s " % self.syslog_ng_path.get_syslog_ng_pid()
        start_command += "-c %s " % self.syslog_ng_path.get_syslog_ng_control_socket()
        start_command += "> %s " % self.syslog_ng_path.get_syslog_ng_console_log()

        if expected_start:
            process = run_command_for_pid(start_command)
            self.process_pid = process.pid
        else:
            assert run_command_for_exit_code(start_command) != 0
            return

        self.syslog_ng_utils.wait_for_starting_process()
        logging.info("syslog-ng has started with pid: %s" % self.process_pid)

    def stop(self):
        if self.syslog_ng_ctl.stop():
            self.syslog_ng_utils.wait_for_stopping_process()
            logging.info("syslog_ng has stopped with pid: %s" % self.process_pid)
            self.process_pid = None
        else:
            logging.info("syslog_ng can not stopped with pid: %s" % self.process_pid)
            sys.exit(1)

    def reload(self):
        if self.syslog_ng_ctl.reload():
            self.syslog_ng_utils.wait_for_reloading_process()
            logging.info("syslog-ng has reloaded with pid: %s" % self.process_pid)
        else:
            logging.info("syslog_ng can not reloaded with pid: %s" % self.process_pid)
            sys.exit(1)

    def get_process_pid(self):
        return self.process_pid
