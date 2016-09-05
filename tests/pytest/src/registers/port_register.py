import random

from src.common.singleton import Singleton
from src.registers.global_register import GlobalRegister


@Singleton
class PortRegister(GlobalRegister):
    def __init__(self):
        GlobalRegister.__init__(self)
        self.registered_tcp_ports = {}
        self.registered_udp_ports = {}
        self.min_port = 49152
        self.max_port = 65535

    def __del__(self):
        pass

    def get_registered_tcp_port(self, prefix):
        return self.get_registered_port(port_group=self.registered_tcp_ports, prefix=prefix)

    def get_registered_udp_port(self, prefix):
        return self.get_registered_port(port_group=self.registered_udp_ports, prefix=prefix)

    def get_registered_port(self, port_group, prefix):
        if self.is_item_registered(key=prefix, group=port_group):
            return self.get_registered_item(key=prefix, group=port_group)
        unique_port = self.create_and_register_unique_port(prefix=prefix, port_group=port_group)
        return unique_port

    def create_and_register_unique_port(self, prefix, port_group):
        unique_port = self.get_unique_port(port_group=port_group)
        self.register_item_in_group(key=prefix, value=unique_port, group=port_group)
        return unique_port

    def get_unique_port(self, port_group):
        while True:
            random_port = random.randint(self.min_port, self.max_port)
            if not self.is_port_registered(port=random_port, port_group=port_group):
                return random_port

    @staticmethod
    def is_port_registered(port, port_group):
        return port in port_group.values()
