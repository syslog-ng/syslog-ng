from src.drivers.file.file_reader import FileReader
from src.drivers.socket.socket_reader import SocketReader
from src.syslog_ng.configuration.config import SyslogNgConfig

class IntuitiveReceiver(object):
    def __init__(self):
        self.config = SyslogNgConfig.Instance()
        self.receiver = Receiver()

    def wait_for_message(self):
        dst_file_paths, dst_socket_paths = self.config.get_paths_for_connected_elements(group_type="destination")

        if dst_file_paths:
            for dst_file_path in dst_file_paths:
                self.receiver.wait_for_message_in_file(file_path=dst_file_path)

        if dst_socket_paths:
            for dst_socket_path in dst_socket_paths:
                self.receiver.wait_for_message_in_socket(ip=dst_socket_path[0], port=dst_socket_path[1])


class Receiver(object):
    def __init__(self):
        self.socket_reader = SocketReader()
        self.file_reader = FileReader()

    def wait_for_message_in_file(self, file_path):
        return self.file_reader.get_messages(file_path=file_path)

    def wait_for_message_in_socket(self, ip, port):
        return self.socket_reader.wait_for_messages_on_tcp(ip=ip, port=port)

