import os
import uuid
import random

class GlobalRegister(object):
    def __init__(self):
        self.registered_ports = []
        self.registered_files = []
        # contains dynamic or private ports that cannot be registered with IANA
        self.min_port = 49152
        self.max_port = 65535
        self.global_prefix = "pytest"

    def get_uniq_filename(self, prefix, extension="txt", subdir="/tmp"):
        unique_file_name = None
        while True:
            unique_file_name = os.path.join(subdir,"%s_%s_%s.%s" % (self.global_prefix, prefix, str(uuid.uuid4()), extension))
            if not self.is_file_name_registered(unique_file_name):
                self.register_file_name(unique_file_name)
                break
        return unique_file_name

    # def get_uniq_dirname(self):
    #     pass

    def get_uniq_port(self):
        random_port = None
        while True:
            if self.is_available_port():
                random_port = random.randint(self.min_port, self.max_port)
                if not self.is_port_registered(random_port):
                    self.register_port(random_port)
                    break
            else:
                break
        return random_port

    def is_port_registered(self, port):
        return port in self.registered_ports

    def register_port(self, port):
        self.registered_ports.append(port)

    def is_available_port(self):
        is_available_port = False
        for i in range(self.min_port, self.max_port):
            if i not in self.registered_ports:
                is_available_port = True
                break
        return is_available_port

    def is_file_name_registered(self, file_name):
        return file_name in self.registered_files

    def register_file_name(self, file_name):
        self.registered_files.append(file_name)

    def delete_registered_files(self):
        for registered_file in self.registered_files:
            if os._exists(registered_file):
                os.unlink(registered_file)
            self.registered_files.remove(registered_file)
