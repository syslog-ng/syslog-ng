import socket
import threading

from src.common.waiters import wait_till_before_after_not_equals
from src.drivers.socket.socket_driver import SocketDriver


class SocketReader(SocketDriver):
    def __init__(self):
        SocketDriver.__init__(self)
        self.socket_timeout = 3
        self.received_messages = {}

    def get_length_for_received_message(self, socket_id):
        return len(self.received_messages[socket_id])

    def wait_for_messages_on_tcp(self, ip, port):
        socket_id = "tcp_%s_%s" % (ip, port)
        self.received_messages[socket_id] = ""
        tcp_thread = threading.Thread(target=self.socket_stream_listener,
                                      kwargs={"address_family": socket.AF_INET, "driver_name": "tcp", "ip": ip, "port": port})
        tcp_thread.daemon = True
        tcp_thread.start()

    def wait_for_messages_on_tcp6(self, ip, port):
        pass

    def wait_for_messages_on_udp(self, ip, port):
        pass

    def wait_for_messages_on_udp6(self, ip, port):
        pass

    def wait_for_messages_on_unix_dgram(self, socket_file_path):
        pass

    def wait_for_messages_on_unix_stream(self, socket_file_path):
        pass

    def wait_till_socket_not_received_more_messages(self, socket_id):
        wait_till_before_after_not_equals(self.get_length_for_received_message, socket_id)

    def get_messages_from_tcp(self, ip, port):
        socket_id = "tcp_%s_%s" % (ip, port)
        self.wait_till_socket_not_received_more_messages(socket_id)
        return self.received_messages[socket_id]

    def socket_stream_listener(self, address_family, driver_name, ip=None, port=None, socket_file_path=None):
        sock = socket.socket(address_family, socket.SOCK_STREAM)
        sock.settimeout(self.socket_timeout)

        if ip:
            server_address = (ip, port)
            socket_id = "%s_%s_%s" % (driver_name, ip, port)
        else:
            server_address = socket_file_path
            socket_id = "%s_%s" % (driver_name, socket_file_path)

        sock.bind(server_address)
        no_more_data = False
        sock.listen(1)
        while True:
            connection, client_address = sock.accept()
            connection.settimeout(self.socket_timeout)
            try:
                while True:
                    data = connection.recv(8192).decode("utf-8")
                    if data != "":
                        self.received_messages[socket_id] += data
                    else:
                        no_more_data = True
                        break
            except socket.timeout:
                break
            finally:
                connection.close()
            if no_more_data:
                break
        print(self.received_messages)

    def socket_dgram_listener(self, address_family, driver_name, ip=None, port=None, socket_file_path=None):
        sock = socket.socket(address_family, socket.SOCK_DGRAM)
        sock.settimeout(self.socket_timeout)
        if ip:
            server_address = (ip, port)
            socket_id = "%s_%s_%s" % (driver_name, ip, port)
        else:
            server_address = socket_file_path
            socket_id = "%s_%s" % (driver_name, socket_file_path)

        sock.bind(server_address)

        while True:
            try:
                if ip:
                    data = sock.recv(8192).decode("utf-8")
                else:
                    data = sock.recv(8192).decode("utf-8")

                if data != "":
                    self.received_messages[socket_id].append(data)
                    break
            except socket.timeout:
                break
        sock.close()
