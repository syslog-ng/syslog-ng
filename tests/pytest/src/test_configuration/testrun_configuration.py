import configparser
import logging
import os

import coloredlogs


class TestdbConfigReader(object):
    def __init__(self):
        configuration_file = os.path.join("/tmp/", "testdb_configuration.ini")
        self.config_parser = configparser.ConfigParser()
        self.config_parser.read(configuration_file)
        self.valid_config_elements = ["runningtest", "syslognginstall", "configversion", "installdir", "loglevel"]
        self.validate_config_elements()

        self.running_test_type = self.config_parser["Testdb"]["RunningTest"]
        self.syslog_ng_install_type = self.config_parser["Testdb"]["SyslogNgInstall"]
        self.config_version = self.config_parser["Testdb"]["ConfigVersion"]
        self.install_dir = self.config_parser["Testdb"]["InstallDir"]
        self.log_level = self.config_parser["Testdb"]["LogLevel"]

        self.set_testrun_logging_level()

    def get_running_test_type(self):
        return self.running_test_type

    def get_syslog_ng_install_type(self):
        return self.syslog_ng_install_type

    def get_install_dir(self):
        return self.install_dir

    def get_log_level(self):
        return self.log_level

    def get_config_version(self):
        return self.config_version

    def set_testrun_logging_level(self):
        if self.log_level == "INFO":
            coloredlogs.install(level=logging.INFO)
        elif self.log_level == "ERROR":
            coloredlogs.install(level=logging.ERROR)
        else:
            raise ValueError('Not supported logging level')

    def validate_config_elements(self):
        for item in self.config_parser["Testdb"]:
            if item not in self.valid_config_elements:
                raise ValueError('Found invalid config element')
