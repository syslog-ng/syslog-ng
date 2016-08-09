import os
from src.testrun.testrun_configuration_reader.testrun_configuration_reader import TestdbConfigReader


class SyslogNgPathHandler(object):

    def __init__(self):
        testdb_config_reader = TestdbConfigReader()
        self.install_type = testdb_config_reader.get_testdb_install_type()
        self.custom_install_path = testdb_config_reader.get_testdb_install_path()
        self.balabitpkg_install_path = "/opt/syslog-ng"

        self.syslog_ng_file_paths = {
            "config": {
                "ospkg": "/etc/syslog-ng/syslog-ng.conf",
                "balabitpkg": os.path.join(self.balabitpkg_install_path, "etc/syslog_ng.conf"),
                "custom": os.path.join(self.custom_install_path, "etc/syslog_ng.conf"),
            },
            "syslog_ng_binary": {
                "ospkg": "/usr/sbin/syslog-ng",
                "balabitpkg": os.path.join(self.balabitpkg_install_path, "sbin/syslog-ng"),
                "custom": os.path.join(self.custom_install_path, "sbin/syslog-ng"),
            },
            "syslog_ng_control_tool": {
                "ospkg": "/usr/sbin/syslog-ng-ctl",
                "balabitpkg": os.path.join(self.balabitpkg_install_path, "sbin/syslog-ng-ctl"),
                "custom": os.path.join(self.custom_install_path, "sbin/syslog-ng-ctl"),
            },
            "syslog_ng_control_socket": {
                "ospkg": "/var/lib/syslog-ng/syslog-ng.ctl",
                "balabitpkg": os.path.join(self.balabitpkg_install_path, "var/syslog-ng.ctl"),
                "custom": os.path.join(self.custom_install_path, "var/syslog-ng.ctl"),
            },
            "console_log": {
                "ospkg": "/root/console.log",
                "balabitpkg": os.path.join(self.balabitpkg_install_path, "sbin/console.log"),
                "custom": os.path.join(self.custom_install_path, "sbin/console.log"),
            },
            "syslog_ng_pid": {
                "ospkg": "/var/lib/syslog-ng/syslog-ng.pid",
                "balabitpkg": os.path.join(self.balabitpkg_install_path, "var/syslog-ng.pid"),
                "custom": os.path.join(self.custom_install_path, "var/syslog-ng.pid"),
            },
            "syslog_ng_persist": {
                "ospkg": "/var/lib/syslog-ng/syslog-ng.persist",
                "balabitpkg": os.path.join(self.balabitpkg_install_path, "var/syslog-ng.persist"),
                "custom": os.path.join(self.custom_install_path, "var/syslog-ng.persist"),
            }
        }

    def get_syslog_ng_config_path(self):
        return self.syslog_ng_file_paths["config"][self.install_type]

    def get_syslog_ng_binary(self):
        return self.syslog_ng_file_paths["syslog_ng_binary"][self.install_type]

    def get_syslog_ng_console_log(self):
        return self.syslog_ng_file_paths["console_log"][self.install_type]

    def get_syslog_ng_pid(self):
        return self.syslog_ng_file_paths["syslog_ng_pid"][self.install_type]

    def get_syslog_ng_persist(self):
        return self.syslog_ng_file_paths["syslog_ng_persist"][self.install_type]

    def get_syslog_ng_control_socket(self):
        return self.syslog_ng_file_paths["syslog_ng_control_socket"][self.install_type]

    def get_syslog_ng_control_tool(self):
        return self.syslog_ng_file_paths["syslog_ng_control_tool"][self.install_type]

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
