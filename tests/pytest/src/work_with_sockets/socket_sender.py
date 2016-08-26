import socket
import logging

class SocketSender(object):
    def __init__(self):
        pass

    def tcp_send(self, ip, port, message):
        self.socket_stream_sender(address_family=socket.AF_INET, message=message, ip=ip, port=port)

    def udp_send(self, ip, port, message):
        self.socket_dgram_sender(address_family=socket.AF_INET, message=message, ip=ip, port=port)

    def unix_stream_send(self, socket_file_path, message):
        self.socket_stream_sender(address_family=socket.AF_UNIX, message=message, socket_file_path=socket_file_path)

    def unix_dgram_send(self, socket_file_path, message):
        self.socket_dgram_sender(address_family=socket.AF_UNIX, message=message, socket_file_path=socket_file_path)

    def tcp6_send(self, ip, port, message):
        self.socket_stream_sender(address_family=socket.AF_INET6, message=message, ip=ip, port=port)

    def udp6_send(self, ip, port, message):
        self.socket_dgram_sender(address_family=socket.AF_INET6, message=message, ip=ip, port=port)

    def socket_stream_sender(self, address_family, message, ip=None, port=None, socket_file_path=None):
        sock = socket.socket(address_family, socket.SOCK_STREAM)

        if ip:
            server_address = (ip, port)
        else:
            server_address = socket_file_path
        sock.connect(server_address)

        for item in message:
            try:
                if sock.sendall(str.encode(item)) is not None:
                    logging.error("Cannot send message: [%s], to server address: [%s]" % (message, server_address))
                else:
                    logging.info("Message sent: [%s], to server address: [%s]" % (message, server_address))
            finally:
                sock.close()

    def socket_dgram_sender(self, address_family, message, ip=None, port=None, socket_file_path=None):
        sock = socket.socket(address_family, socket.SOCK_DGRAM)

        if ip:
            server_address = (ip, port)
        else:
            server_address = socket_file_path
        sock.sendto(message, (server_address))