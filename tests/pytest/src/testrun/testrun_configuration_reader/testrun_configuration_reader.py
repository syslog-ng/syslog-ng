import configparser
import coloredlogs
import logging

class TestdbConfigReader(object):

    def __init__(self):
        config = configparser.ConfigParser()
        config.read("testdb_configuration.ini")
        self.install_type = config["Testdb"]["Type"]
        self.install_path = config["Testdb"]["InstallDir"]
        self.version = config["Testdb"]["Version"]
        self.log_level = config["Testdb"]["LogLevel"]
        self.set_testrun_logging_level()

    def get_testdb_install_type(self):
        return self.install_type

    def get_testdb_version(self):
        return self.version

    def get_testdb_install_path(self):
        return self.install_path

    def get_testdb_log_level(self):
        return self.log_level

    def set_testrun_logging_level(self):
        if self.log_level == "DEBUG":
            coloredlogs.install(level=logging.DEBUG)
            logging.basicConfig(level=logging.DEBUG)
        elif self.log_level == "INFO":
            coloredlogs.install(level=logging.INFO)
        elif self.log_level == "ERROR":
            coloredlogs.install(level=logging.ERROR)
        else:
            coloredlogs.install(level=logging.DEBUG)
