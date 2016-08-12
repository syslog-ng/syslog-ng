import os
import uuid
import random

class GlobalRegister(object):
    def __init__(self):
        self.registered_tcp_ports = {}
        self.registered_udp_ports = {}
        self.registered_files = []
        self.registered_dirs = []
        # contains dynamic or private ports that cannot be registered with IANA
        self.min_port = 49152
        self.max_port = 65535
        self.global_prefix = "pytest"
        self.global_rootdir = "/tmp"

    def __del__(self):
        self.delete_registered_files()
        self.delete_registered_dirs()
        self.delete_registered_tcp_ports()
        self.delete_registered_udp_ports()

    ### FILE

    def get_uniq_filename(self, prefix, extension="txt", under_subdir=None):
        if not under_subdir:
            under_subdir = self.global_rootdir
        else:
            under_subdir = os.path.join(self.global_rootdir, under_subdir)
            self.register_dirname(under_subdir)
        unique_file_name = None
        while True:
            unique_file_name = os.path.join(under_subdir, "%s_%s_%s.%s" % (self.global_prefix, prefix, str(uuid.uuid4()), extension))
            if (not self.is_filename_registered(unique_file_name)) and (not self.is_file_exist(unique_file_name)):
                self.register_filename(unique_file_name)
                break
        return unique_file_name

    def get_uniq_dirname(self, prefix, under_subdir=None):
        if not under_subdir:
            under_subdir = self.global_rootdir
        else:
            under_subdir = os.path.join(self.global_rootdir, under_subdir)
            self.register_dirname(under_subdir)
        unique_dir_name = None
        while True:
            unique_dir_name = os.path.join(under_subdir, "%s_%s_%s" % (self.global_prefix, prefix, str(uuid.uuid4())))
            if (not self.is_dirname_registered(unique_dir_name)) and (not self.is_file_exist(unique_dir_name)):
                self.register_dirname(unique_dir_name)
                break
        return unique_dir_name

    def is_file_exist(self, file_name):
        return os.path.exists(file_name)

    def is_filename_registered(self, file_name):
        return file_name in self.registered_files

    def is_dirname_registered(self, dir_name):
        return dir_name in self.registered_dirs

    def register_filename(self, file_name):
        self.registered_files.append(file_name)

    def register_dirname(self, dir_name):
        self.registered_dirs.append(dir_name)

    def delete_registered_files(self):
        for registered_file in self.registered_files:
            if os.path.exists(registered_file):
                os.unlink(registered_file)
        self.registered_files = []

    def delete_registered_dirs(self):
        for registered_dir in self.registered_dirs:
             if os.path.exists(registered_dir):
                os.rmdir(registered_dir)
        self.registered_dirs = []

    ### PORT

    def get_uniq_tcp_port(self, group_name):
        return self.get_uniq_port(group_name=group_name, port_group=self.registered_tcp_ports)

    def get_uniq_udp_port(self, group_name):
        return self.get_uniq_port(group_name=group_name, port_group=self.registered_udp_ports)

    def get_uniq_port(self, group_name, port_group):
        random_port = None
        while True:
            if self.is_available_port(port_group=port_group):
                random_port = random.randint(self.min_port, self.max_port)
                if not self.is_port_registered(port=random_port, port_group=port_group):
                    self.register_port(port=random_port, port_group=port_group, group_name=group_name)
                    break
            else:
                break
        return random_port

    def is_port_registered(self, port, port_group):
        port_found = False
        for item in port_group:
            if port_group[item] == port:
                port_found = True
        return port_found

    def register_port(self, port, port_group, group_name):
        port_group[group_name] = port

    def is_available_port(self, port_group):
        is_available_port = False
        for i in range(self.min_port, self.max_port):
            if i not in port_group.values():
                is_available_port = True
                break
        return is_available_port

    def delete_registered_tcp_ports(self):
        self.registered_tcp_ports = {}

    def delete_registered_udp_ports(self):
        self.registered_udp_ports = {}
