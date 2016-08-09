import socket


# from src.syslog_ng_options_and_drivers_database.slng_config_database import socket_database

def get_socket_information(driver_name, driver_type):
    address_family = socket_database[driver_name].get('address_family')
    socket_type = socket_database[driver_name].get('socket_type')
    local_ip_address = socket_database[driver_name].get('local_ip')
    if driver_type == "source":
        local_port = socket_database[driver_name].get('source_port')
    else:
        local_port = socket_database[driver_name].get('destination_port')
    if "unix" in driver_name:
        return address_family, socket_type
    else:
        return address_family, socket_type, local_ip_address, local_port


### Senders
def send_message_to_stream_socket(source_driver_name, message, socket_file_path=None):
    if "unix" in source_driver_name:
        address_family, socket_type = get_socket_information(source_driver_name, "source")
        client_socket = socket.socket(address_family, socket_type)
        client_socket.connect(socket_file_path)
    else:
        address_family, socket_type, local_ip_address, local_port = get_socket_information(source_driver_name, "source")
        client_socket = socket.socket(address_family, socket_type)
        client_socket.connect((local_ip_address, int(local_port)))

    if client_socket.sendall(message) is not None:
        assert False
    client_socket.close()


def send_message_to_dgram_socket(source_driver_name, message, socket_file_path=None):
    if "unix" in source_driver_name:
        address_family, socket_type = get_socket_information(source_driver_name, "source")
        client_socket = socket.socket(address_family, socket_type)
        client_socket.connect(socket_file_path)
    else:
        address_family, socket_type, local_ip_address, local_port = get_socket_information(source_driver_name, "source")
        client_socket = socket.socket(address_family, socket_type)
        client_socket.connect((local_ip_address, int(local_port)))

    client_socket.sendall(message)
    client_socket.close()


### Listeners
def start_stream_listener(destination_driver_name, socket_file_path=None):
    if "unix" in destination_driver_name:
        address_family, socket_type = get_socket_information(destination_driver_name, "destination")
        server_socket = socket.socket(address_family, socket_type)
        server_socket.bind(socket_file_path)
    else:
        address_family, socket_type, local_ip_address, local_port = get_socket_information(destination_driver_name,
                                                                                           "destination")
        server_socket = socket.socket(address_family, socket_type)
        server_socket.bind((local_ip_address, int(local_port)))

    server_socket.listen(5)
    server_socket.settimeout(2)
    output_messages = []

    while 1:
        try:
            connection, client_address = server_socket.accept()
            while 1:
                data = connection.recv(2048)
                if not data:
                    break
                output_messages.append(data)
        except socket.timeout:
            break
        connection.close()

    return output_messages


def start_dgram_listener(destination_driver_name, socket_file_path=None):
    if "unix" in destination_driver_name:
        address_family, socket_type = get_socket_information(destination_driver_name, "destination")
        server_socket = socket.socket(address_family, socket_type)
        server_socket.bind(socket_file_path)
    else:
        address_family, socket_type, local_ip_address, local_port = get_socket_information(destination_driver_name,
                                                                                           "destination")
        server_socket = socket.socket(address_family, socket_type)
        server_socket.bind((local_ip_address, int(local_port)))

    server_socket.settimeout(2)
    output_messages = []

    while 1:
        try:
            data, client_address = server_socket.recvfrom(2048)
            if not data:
                break
            output_messages.append(data)
        except socket.timeout:
            break
        server_socket.close()

    return output_messages
