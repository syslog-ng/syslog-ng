import sys
from src.syslog_ng.path_handler.path_handler import SyslogNgPathHandler
from src.syslog_ng.binary_syslog_ng_ctl.syslog_ng_ctl_utils import *
from src.work_with_process import run_command


class SyslogNg(object):
    def __init__(self):
        self.process_pid = None
        self.global_config = None

    def set_global_config(self, global_config):
        self.global_config = global_config
        self.syslog_ng_path = self.global_config['syslog_ng_path']

    def start(self):
        start_command = "%s " % self.syslog_ng_path.get_syslog_ng_binary()
        start_command += "-Ftv --no-caps --enable-core "
        start_command += "-f %s " % self.syslog_ng_path.get_syslog_ng_config_path()
        start_command += "-R %s " % self.syslog_ng_path.get_syslog_ng_persist()
        start_command += "-p %s " % self.syslog_ng_path.get_syslog_ng_pid()
        start_command += "-c %s " % self.syslog_ng_path.get_syslog_ng_control_socket()
        start_command += "> %s " % self.syslog_ng_path.get_syslog_ng_console_log()

        process = run_command.run_command_for_pid(start_command)
        self.process_pid = process.pid

        if self.global_config['config'].added_internal_source:
           self.global_config['syslog_ng_utils'].wait_for_starting_up()
        logging.info("syslog-ng has started with pid: %s" % self.process_pid)

    def stop(self):
        if self.global_config['syslog_ng_ctl'].stop():
            if self.global_config['config'].added_internal_source:
                self.global_config['syslog_ng_utils'].wait_for_shutting_down()
            logging.info("syslog_ng has stopped with pid: %s" % self.process_pid)
            self.process_pid = None
        else:
            logging.info("syslog_ng can not stopped with pid: %s" % self.process_pid)
            self.global_config['syslog_ng_utils'].dump_syslog_ng_console_log()
            sys.exit(1)

    def reload(self):
        if self.global_config['syslog_ng_ctl'].reload():
            if self.global_config['config'].added_internal_source:
                self.global_config['syslog_ng_utils'].wait_for_reloading()
            logging.info("syslog-ng has reloaded with pid: %s" % self.process_pid)
        else:
            logging.info("syslog_ng can not reloaded with pid: %s" % self.process_pid)
            sys.exit(1)

    def get_process_pid(self):
        return self.process_pid
