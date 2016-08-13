import os
import socket


class SocketListener(object):
    def __init__(self):
        self.received_messsages = {"tcp": [], "network": [], "syslog": [], "udp": [], "unix_stream": [], "unix_dgram": [], "tcp6": [], "udp6": []}
        self.current_driver = None
        self.socket_timeout = 5

    def get_received_messages(self):
        return self.received_messsages

    def tcp_listener(self, ip, port):
        self.current_driver = "tcp"
        self.socket_stream_listener(socket.AF_INET, ip=ip, port=port)

    def udp_listener(self, ip, port):
        self.current_driver = "udp"
        self.socket_dgram_listener(socket.AF_INET, ip=ip, port=port)

    def unix_stream_listener(self, socket_file_path):
        self.current_driver = "unix_stream"
        self.socket_stream_listener(socket.AF_UNIX, socket_file_path=socket_file_path)

    def unix_dgram_listener(self, socket_file_path):
        self.current_driver = "unix_dgram"
        self.socket_dgram_listener(socket.AF_UNIX, socket_file_path=socket_file_path)

    def tcp6_listener(self, ip, port):
        self.current_driver = "tcp6"
        self.socket_stream_listener(socket.AF_INET6, ip=ip, port=port)

    def udp6_listener(self, ip, port):
        self.current_driver = "udp6"
        self.socket_dgram_listener(socket.AF_INET6, ip=ip, port=port)

    def socket_stream_listener(self, address_family, ip=None, port=None, socket_file_path=None):
        sock = socket.socket(address_family, socket.SOCK_STREAM)
        sock.settimeout(self.socket_timeout)

        if ip:
            server_address = (ip, port)
        else:
            server_address = (socket_file_path)
            if os.path.exists(socket_file_path):
                os.remove(socket_file_path)

        sock.bind(server_address)
        no_more_data = False
        sock.listen(1)
        while True:
            connection, client_address = sock.accept()
            connection.settimeout(self.socket_timeout)
            try:
                while True:
                    data = connection.recv(8192).decode("utf-8")
                    print("received on tcp :%s" % data)
                    if data != "":
                        self.received_messsages[self.current_driver].append(data)
                    else:
                        no_more_data = True
                        break
            except socket.timeout:
                break
            finally:
                connection.close()
            if no_more_data:
                break

        if socket_file_path:
            os.remove(socket_file_path)


    def socket_dgram_listener(self, address_family, ip=None, port=None, socket_file_path=None):
        sock = socket.socket(address_family, socket.SOCK_DGRAM)
        sock.settimeout(self.socket_timeout)
        if ip:
            server_address = (ip, port)
        else:
            server_address = (socket_file_path)
            if os.path.exists(socket_file_path):
                os.remove(socket_file_path)

        sock.bind(server_address)

        while True:
            try:
                if ip:
                    data = sock.recv(1024)
                    print("received on udp: %s" %data)
                else:
                    data = sock.recv(1024)

                if data != "":
                    self.received_messsages[self.current_driver].append(data)
                    break
            except socket.timeout:
                break

        sock.close()

        if socket_file_path:
            os.remove(socket_file_path)