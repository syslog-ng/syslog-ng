class SocketDriver(object):
    def __init__(self):
        pass

    @staticmethod
    def socket_connect(socket, server_address):
        socket.connect(server_address)
