import logging
import socket

from src.common.waiters import wait_till_statement_not_false
from src.drivers.socket.socket_driver import SocketDriver


class SocketWriter(SocketDriver):
    def __init__(self):
        SocketDriver.__init__(self)

    def send_message_to_tcp(self, ip, port, message):
        self.socket_stream_sender(address_family=socket.AF_INET, message=message, ip=ip, port=port)

    def socket_stream_sender(self, address_family, message, ip=None, port=None, socket_file_path=None):
        sock = socket.socket(address_family, socket.SOCK_STREAM)

        if ip:
            server_address = (ip, port)
        else:
            server_address = socket_file_path

        wait_till_statement_not_false(self.socket_connect(sock, server_address))

        try:
            if sock.sendall(str.encode(message)) is not None:
                logging.error("Cannot send message: [%s], to server address: [%s]" % (message, server_address))
            else:
                logging.info("Message sent: [%s], to server address: [%s]" % (message, server_address))
        finally:
            sock.close()

    @staticmethod
    def socket_dgram_sender(address_family, message, ip=None, port=None, socket_file_path=None):
        sock = socket.socket(address_family, socket.SOCK_DGRAM)

        if ip:
            server_address = (ip, port)
        else:
            server_address = socket_file_path

        sock.sendto(str.encode(message), server_address)
