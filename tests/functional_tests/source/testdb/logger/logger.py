import logging
import sys

from colorlog import ColoredFormatter


class TestdbLogger(object):
    def __init__(self, testdb_path_database, testdb_config_context):
        self.testcase_report_file = testdb_path_database.testcase_report_file
        self.defined_log_level = testdb_config_context.log_level

        self.log_level = self.__set_log_level(self.defined_log_level)

    @staticmethod
    def __set_log_level(defined_log_level):
        if defined_log_level == "info":
            return logging.INFO
        elif defined_log_level == "error":
            return logging.ERROR
        elif defined_log_level == "debug":
            return logging.DEBUG
        else:
            print("Unknown defined loglevel: [%s]" % defined_log_level)
            assert False

    def set_logger(self, logsource="Logger"):
        log_writer = logging.getLogger(logsource)
        log_writer.handlers = []
        log_writer.setLevel(self.log_level)
        console_handler = self.__set_consolehandler()
        log_writer.addHandler(console_handler)

        file_handler = self.__set_filehandler(file_name=self.testcase_report_file)
        log_writer.addHandler(file_handler)
        return log_writer

    def __set_filehandler(self, file_name=None):
        file_handler = logging.FileHandler(file_name)
        file_handler.setLevel(self.log_level)
        formatter = logging.Formatter('%(asctime)s - %(name)s - %(levelname)s - %(message)s')
        file_handler.setFormatter(formatter)
        return file_handler

    def __set_consolehandler(self):
        console_handler = logging.StreamHandler(sys.stdout)
        console_handler.setLevel(self.log_level)
        formatter = ColoredFormatter(
            "\n%(log_color)s%(asctime)s - %(name)s - %(levelname)-5s%(reset)s- %(message_log_color)s%(message)s",
            datefmt=None,
            reset=True,
            log_colors={
                'DEBUG':    'cyan',
                'INFO':     'green',
                'WARNING':  'yellow',
                'ERROR':    'red',
                'CRITICAL': 'red,bg_white',
            },
            secondary_log_colors={
                'message': {
                    'ERROR':    'red',
                    'CRITICAL': 'red'
                }
            },
            style='%'
        )
        console_handler.setFormatter(formatter)
        return console_handler

    @staticmethod
    def write_message_based_on_value(logsource, message, value, positive_loglevel="debug"):
        if value:
            if positive_loglevel == "debug":
                logsource.debug("%s: [%s]" % (message, value))
            else:
                logsource.info("%s: [%s]" % (message, value))
        else:
            logsource.error("%s: [%s]" % (message, value))
