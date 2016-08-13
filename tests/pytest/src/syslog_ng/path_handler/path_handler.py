import os
from src.testrun.testrun_configuration_reader.testrun_configuration_reader import TestdbConfigReader


class SyslogNgPathHandler(object):
    def __init__(self):
        testdb_config_reader = TestdbConfigReader()
        self.syslog_ng_install = testdb_config_reader.get_syslog_ng_install()
        self.install_dir = testdb_config_reader.get_install_dir()
        self.balabitpkg_install_path = "/opt/syslog-ng"
        self.temporary_working_dir = "/tmp"
        self.config_file = os.path.join(self.temporary_working_dir, "syslog-ng.conf")
        self.control_socket = os.path.join(self.temporary_working_dir, "syslog-ng.ctl")
        self.console_log = os.path.join(self.temporary_working_dir, "console.log")
        self.syslog_ng_pid = os.path.join(self.temporary_working_dir, "syslog-ng.pid")
        self.syslog_ng_persist = os.path.join(self.temporary_working_dir, "syslog-ng.persist")

        self.syslog_ng_file_paths = {
            "syslog_ng_binary": {
                "path": "syslog-ng",
                "ospkg": "/usr/sbin/syslog-ng",
                "balabitpkg": os.path.join(self.balabitpkg_install_path, "sbin/syslog-ng"),
                "custom": os.path.join(self.install_dir, "sbin/syslog-ng"),
            },
            "syslog_ng_control_tool": {
                "path": "syslog-ng-ctl",
                "ospkg": "/usr/sbin/syslog-ng-ctl",
                "balabitpkg": os.path.join(self.balabitpkg_install_path, "sbin/syslog-ng-ctl"),
                "custom": os.path.join(self.install_dir, "sbin/syslog-ng-ctl"),
            },
        }

        self.global_config = None

    def set_global_config(self, global_config):
        self.global_config = global_config
        self.global_register = self.global_config['global_register']

    def get_syslog_ng_binary(self):
        return self.syslog_ng_file_paths["syslog_ng_binary"][self.syslog_ng_install]

    def get_syslog_ng_control_tool(self):
        return self.syslog_ng_file_paths["syslog_ng_control_tool"][self.syslog_ng_install]

    def get_syslog_ng_config_path(self):
        return self.config_file

    def get_syslog_ng_console_log(self):
        return self.console_log

    def get_syslog_ng_pid(self):
        return self.syslog_ng_pid

    def get_syslog_ng_persist(self):
        return self.syslog_ng_persist

    def get_syslog_ng_control_socket(self):
        return self.control_socket

    def get_session_files(self):
        return [
            self.get_syslog_ng_console_log(),
            self.get_syslog_ng_persist(),
            self.get_syslog_ng_pid(),
            self.get_syslog_ng_control_socket()
        ]

    def is_syslog_ng_binary_exist(self):
        return os.path.exists(self.get_syslog_ng_binary())

    def is_syslog_ng_pid_exist(self):
        return os.path.exists(self.get_syslog_ng_pid())
