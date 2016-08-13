from src.syslog_ng.binary_syslog_ng.binary_syslog_ng import SyslogNg
from src.syslog_ng.config_generator.config_generator import ConfigGenerator
from src.work_with_files.file_based_processor import FileBasedProcessor
from src.testrun.testrun_reporter.testrun_reporter import TestrunReporter
from src.send_receive_logs.log_sender.log_sender import LogSender
from src.send_receive_logs.log_receiver.log_receiver import LogReceiver
from src.syslog_ng.binary_syslog_ng.syslog_ng_utils import SyslogNgUtils
from src.testrun.testrun_configuration_reader.testrun_configuration_reader import TestdbConfigReader
from src.syslog_ng.binary_syslog_ng_ctl.binary_syslog_ng_ctl import SyslogNgCtl
from src.syslog_ng.binary_syslog_ng_ctl.syslog_ng_ctl_utils import SyslogNgCtlUtils
from src.global_register.global_register import GlobalRegister
from src.syslog_ng.path_handler.path_handler import SyslogNgPathHandler
from src.send_receive_logs.log_generator.log_generator import MessageGenerator


class TC(object):
    def __init__(self):
        TestdbConfigReader()
        self.filemanager = FileBasedProcessor()
        self.config = ConfigGenerator()
        self.test_reporter = TestrunReporter()
        self.sender = LogSender()
        self.receiver = LogReceiver()
        self.syslog_ng = SyslogNg()
        self.syslog_ng_utils = SyslogNgUtils()
        self.syslog_ng_ctl = SyslogNgCtl()
        self.syslog_ng_ctl_utils = SyslogNgCtlUtils()
        self.global_register = GlobalRegister()
        self.syslog_ng_path = SyslogNgPathHandler()
        self.message_generator = MessageGenerator()
        self.global_config = {
            "config": self.config,
            "filemanager": self.filemanager,
            "testrun_reporter": self.test_reporter,
            "sender": self.sender,
            "receiver": self.receiver,
            "syslog_ng": self.syslog_ng,
            "syslog_ng_utils": self.syslog_ng_utils,
            "syslog_ng_ctl": self.syslog_ng_ctl,
            "syslog_ng_ctl_utils": self.syslog_ng_ctl_utils,
            "syslog_ng_path": self.syslog_ng_path,
            "global_register": self.global_register,
            "message_generator": self.message_generator
        }

        self.filemanager.set_global_config(self.global_config)
        self.config.set_global_config(self.global_config)
        self.test_reporter.set_global_config(self.global_config)
        self.sender.set_global_config(self.global_config)
        self.receiver.set_global_config(self.global_config)
        self.syslog_ng.set_global_config(self.global_config)
        self.syslog_ng_utils.set_global_config(self.global_config)
        self.syslog_ng_ctl.set_global_config(self.global_config)
        self.global_register.set_global_config(self.global_config)
        self.message_generator.set_global_config(self.global_config)
        self.syslog_ng_path.set_global_config(self.global_config)
        self.syslog_ng_ctl_utils.set_global_config(self.global_config)