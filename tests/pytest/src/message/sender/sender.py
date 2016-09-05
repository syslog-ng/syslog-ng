from src.drivers.file.file_writer import FileWriter
from src.drivers.socket.socket_writer import SocketWriter
from src.syslog_ng.configuration.config import SyslogNgConfig
from src.syslog_ng.syslog_ng_ctl.syslog_ng_ctl import SyslogNgCtl
from src.syslog_ng.syslog_ng_utils.utils import SyslogNgUtils


class IntuitiveSender(object):
    def __init__(self):
        self.config = SyslogNgConfig.Instance()
        self.sender = Sender()
        self.syslog_ng_ctl = SyslogNgCtl()

    def send_message2(self, message):
        initial_stats = self.syslog_ng_ctl.stats()
        src_file_paths, src_socket_paths = self.config.get_paths_for_connected_elements(group_type="source")

        if src_file_paths:
            for src_file_path in src_file_paths:
                self.sender.send_message_to_file(message=message, file_path=src_file_path, initial_stats=initial_stats)

        if src_socket_paths:
            for src_socket_path in src_socket_paths:
                self.sender.send_message_to_socket(message=message, ip=src_socket_path[0], port=src_socket_path[1])


class Sender(object):
    def __init__(self):
        self.file_writer = FileWriter()
        self.socket_writer = SocketWriter()
        self.syslog_ng_utils = SyslogNgUtils()
        self.syslog_ng_ctl = SyslogNgCtl()

    def send_message_to_file(self, message, file_path, initial_stats):
        self.file_writer.create_file_with_content(file_path=file_path["file_path"], content=message)
        src_group = file_path["group_name"]
        expected_number_of_messages = message.count("\n")
        initial_stats_counter_for_group = 0

        for i in range(0, 1000):
            if "source;%s;;a;processed;%s" % (src_group, i) in initial_stats.decode("utf-8"):
                initial_stats_counter_for_group = i

        expected_number = initial_stats_counter_for_group + expected_number_of_messages

        expected_stats_pattern = "source;%s;;a;processed;%s" % (src_group, expected_number)
        for i in range(0, 1000):
            if expected_stats_pattern in self.syslog_ng_ctl.stats().decode("utf-8"):
                break


    def send_message_to_socket(self, message, ip, port):
        self.socket_writer.send_message_to_tcp(ip=ip, port=port, message=message)
