import binascii
import os
import sys

from src.common.singleton import Singleton
from src.registers.file_register import FileRegister
from src.registers.port_register import PortRegister
from src.syslog_ng.configuration.writer import ConfigWriter


@Singleton
class SyslogNgConfig(object):
    def __init__(self, use_internal_source=True):
        self.config_elements = {
            "version": "3.8",
            "global_options": {},
            "source_groups": [],
            "parser_groups": [],
            "template_groups": [],
            "rewrite_groups": [],
            "filter_groups": [],
            "destination_groups": [],
            "logpaths": [],
        }
        self.file_register = FileRegister.Instance()
        self.port_register = PortRegister.Instance()
        self.config_path = self.file_register.get_registered_filename(prefix="config", extension="conf")
        self.config_writer = ConfigWriter(self.config_elements, self.config_path)
        self.all_source_drivers = ["file", "internal", "network", "nodejs", "pacct", "pipe", "program", "sun_streams", "tcp", "tcp6", "udp", "udp6",
                                   "syslog", "system", "systemd_journal", "systemd_syslog", "unix_dgram", "unix_stream"]
        self.all_destination_drivers = ["amqp", "elasticsearch", "elasticsearch2", "file", "graphite", "hdfs", "http",
                                        "kafka", "loggly", "logmatic", "mongodb", "network", "pipe", "program", "redis", "tcp", "tcp6", "udp", "udp6",
                                        "riemann", "smtp", "sql", "stomp", "syslog", "unix_dgram", "unix_stream",
                                        "usertty"]
        if use_internal_source:
            self.added_internal_source = True
            self.internal_log_file_path = self.file_register.get_registered_filename(prefix="dst_file_internal")
            self.generate_config_with_drivers(source_driver="internal", destination_driver="file")
        else:
            self.added_internal_source = False
        self.connected_elements = []

    def write_config(self):
        self.generate_connected_elements()
        self.config_writer.write_config()

    def get_paths_for_connected_elements(self, group_type):
        file_paths = []
        socket_paths = []
        for _i in range(0, len(self.connected_elements)):
            node_for_selected_group_type = self.connected_elements[_i]["%s_groups" % group_type]
            for _k in range(0, len(node_for_selected_group_type)):
                try:
                    if "file_path" in node_for_selected_group_type[_k]:
                        file_paths.append({"group_name": node_for_selected_group_type[_k]["group_name"], "file_path": node_for_selected_group_type[_k]["file_path"]})
                    if ("ip" in node_for_selected_group_type[_k]) and ("port" in node_for_selected_group_type[_k]):
                        socket_paths.append({"group_name": node_for_selected_group_type[_k]["group_name"], "ip": node_for_selected_group_type[_k]["ip"], "port": node_for_selected_group_type[_k]["port"]})
                except KeyError:
                    continue
        return file_paths, socket_paths

    def generate_connected_elements(self):
        for logpath in self.config_elements["logpaths"]:
            source_group_info = []
            destination_group_info = []
            for source_driver in logpath["source_drivers"]:
                src_group_name = source_driver
                src_driver_name = source_driver.split("_")[1]
                if src_driver_name in ["internal"]:
                    source_group_info.append({"driver_name": src_driver_name, "group_name": src_group_name})
                elif src_driver_name in ["file", "pipe", "program"]:
                    src_file_path = self.get_file_path_for_group(group_type="source", group_name=src_group_name,
                                                                 driver_name=src_driver_name)
                    source_group_info.append({"driver_name": src_driver_name, "group_name": src_group_name, "file_path": src_file_path})
                elif src_driver_name in ["tcp", "udp"]:
                    src_ip, src_port = self.get_socket_path_for_group(group_type="source", group_name=src_group_name,
                                                                      driver_name=src_driver_name)
                    source_group_info.append({"driver_name": src_driver_name, "group_name": src_group_name, "ip": src_ip, "port": src_port})

            for destination_driver in logpath["destination_drivers"]:
                dst_group_name = destination_driver
                dst_driver_name = destination_driver.split("_")[1]
                if dst_driver_name in ["file", "pipe", "program"]:
                    dst_file_path = self.get_file_path_for_group(group_type="destination", group_name=dst_group_name,
                                                                 driver_name=dst_driver_name)
                    destination_group_info.append({"driver_name": dst_driver_name, "group_name": dst_group_name, "file_path": dst_file_path})
                elif dst_driver_name in ["tcp", "udp"]:
                    dst_ip, dst_port = self.get_socket_path_for_group(group_type="destination",
                                                                      group_name=dst_group_name,
                                                                      driver_name=dst_driver_name)
                    destination_group_info.append({"driver_name": dst_driver_name, "group_name": dst_group_name, "ip": dst_ip, "port": dst_port})
            self.connected_elements.append(
                {"source_groups": source_group_info, "destination_groups": destination_group_info})

    def generate_config_with_drivers(self, source_driver, destination_driver):
        src_group = self.add_source_driver(driver_name=source_driver, auto_mandatory_option=True)
        if source_driver == "internal":
            dst_group = self.add_destination_driver(driver_name=destination_driver, auto_mandatory_option=True,
                                                    internal_destination=True)
        else:
            dst_group = self.add_destination_driver(driver_name=destination_driver, auto_mandatory_option=True)
        self.add_logpath([src_group], [dst_group])
        return src_group, dst_group

    def get_socket_path_for_group(self, group_type, group_name, driver_name):
        drivers_node_for_group_name = self.get_drivers_node_for_group_name(group_type=group_type, group_name=group_name)
        driver_options_for_driver_name = self.get_options_node_for_driver_name(drivers_node=drivers_node_for_group_name,
                                                                               driver_name=driver_name)
        ip = self.get_option_value_for_option_name(options_node=driver_options_for_driver_name, option_name="ip")
        port = self.get_option_value_for_option_name(options_node=driver_options_for_driver_name, option_name="port")
        return ip, port

    def get_file_path_for_group(self, group_type, group_name, driver_name):
        drivers_node_for_group_name = self.get_drivers_node_for_group_name(group_type=group_type, group_name=group_name)
        driver_options_for_driver_name = self.get_options_node_for_driver_name(drivers_node=drivers_node_for_group_name,
                                                                               driver_name=driver_name)
        return self.get_option_value_for_option_name(options_node=driver_options_for_driver_name,
                                                     option_name="file_path")

    def get_drivers_node_for_group_name(self, group_type, group_name):
        for group in self.config_elements["%s_groups" % group_type]:
            if group["group_name"] == group_name:
                return group["drivers"]

    @staticmethod
    def get_options_node_for_driver_name(drivers_node, driver_name):
        for driver in drivers_node:
            if driver["driver_name"] == driver_name:
                return driver["driver_options"]

    @staticmethod
    def get_option_value_for_option_name(options_node, option_name):
        return options_node[option_name]

    @staticmethod
    def generate_unique_group_name(group_type, driver_name):
        return "%s_%s_%s" % (group_type, driver_name, binascii.b2a_hex(os.urandom(5)).decode('ascii'))

    def add_global_options(self, options):
        if self.config_elements["global_options"] != {}:
            self.config_elements["global_options"].update(options)
        else:
            self.config_elements["global_options"] = options

    def add_group(self, group_type, driver_name, options, uniq_group_name):
        self.config_elements["%s_groups" % group_type].append(
            {
                "group_name": uniq_group_name,
                "drivers": [
                    {
                        "driver_name": driver_name,
                        "driver_options": options
                    }
                ]
            }
        )
        return uniq_group_name

    # Note: source drivers
    def add_source_driver(self, driver_name, options=None, auto_mandatory_option=False):
        if driver_name not in self.all_source_drivers:
            sys.exit(1)
        uniq_group_name = self.generate_unique_group_name("src", driver_name)
        if auto_mandatory_option and (driver_name in ["file", "pipe", "program", "unix_stream", "unix_dgram"]):
            options = {"file_path": self.file_register.get_registered_filename(prefix=uniq_group_name)}
        elif auto_mandatory_option and (driver_name in ["tcp", "network", "syslog"]):
            options = {"ip": "127.0.0.1", "port": self.port_register.get_registered_tcp_port(prefix=uniq_group_name)}
        return self.add_group(group_type="source", driver_name=driver_name, options=options,
                              uniq_group_name=uniq_group_name)

    def add_destination_driver(self, driver_name, options=None, auto_mandatory_option=False,
                               internal_destination=False):
        if driver_name not in self.all_destination_drivers:
            sys.exit(1)
        uniq_group_name = self.generate_unique_group_name("dst", driver_name)
        if auto_mandatory_option and (driver_name in ["file", "pipe", "program", "unix_stream", "unix_dgram"]):
            if internal_destination:
                file_path = self.internal_log_file_path
            else:
                file_path = self.file_register.get_registered_filename(prefix=uniq_group_name)
            options = {"file_path": file_path}
        elif auto_mandatory_option and (driver_name in ["tcp", "network", "syslog"]):
            options = {"ip": "127.0.0.1", "port": self.port_register.get_registered_tcp_port(prefix=uniq_group_name)}

        return self.add_group(group_type="destination", driver_name=driver_name, options=options,
                              uniq_group_name=uniq_group_name)

    # Note: logpath
    def add_logpath(self, sources, destinations):
        self.config_elements["logpaths"].append({
            "source_drivers": sources,
            "destination_drivers": destinations,
        })

    def get_internal_log_file_path(self):
        return self.internal_log_file_path
