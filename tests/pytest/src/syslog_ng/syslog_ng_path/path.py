import os

from src.registers.file_register import FileRegister
from src.test_configuration.testrun_configuration import TestdbConfigReader


class SyslogNgPath(object):
    def __init__(self):
        testdb_config_reader = TestdbConfigReader()
        self.syslog_ng_install = testdb_config_reader.get_syslog_ng_install_type()
        self.install_dir = testdb_config_reader.get_install_dir()
        self.balabitpkg_install_path = "/opt/syslog-ng"
        self.temporary_working_dir = "/tmp"

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
        self.file_register = FileRegister.Instance()

    def get_syslog_ng_binary(self):
        return self.syslog_ng_file_paths["syslog_ng_binary"][self.syslog_ng_install]

    def get_syslog_ng_control_tool(self):
        return self.syslog_ng_file_paths["syslog_ng_control_tool"][self.syslog_ng_install]

    def get_syslog_ng_config_path(self):
        return self.file_register.get_registered_filename(prefix="config", extension="conf")

    def get_syslog_ng_console_log(self):
        return self.file_register.get_registered_filename(prefix="console_log", extension="log")

    def get_syslog_ng_pid(self):
        return self.file_register.get_registered_filename(prefix="pid", extension="pid")

    def get_syslog_ng_persist(self):
        return self.file_register.get_registered_filename(prefix="persist", extension="persist")

    def get_syslog_ng_control_socket(self):
        return self.file_register.get_registered_filename(prefix="control_socket", extension="ctl")

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
