import binascii
import os
import pystache
import logging
import sys
from src.testrun.testrun_configuration_reader.testrun_configuration_reader import TestdbConfigReader

class ConfigGenerator(object):
    def __init__(self):
        testdb_config_reader = TestdbConfigReader()
        self.config = {
            "version": "%s" % testdb_config_reader.get_config_version(),
            "global_options": [],
            "source_groups": [],
            "parser_groups": [],
            "destination_groups": [],
            "logpath": [],
        }
        self.global_config = None
        self.connected_config_elements = None
        self.added_internal_source = False
        self.registered_source_groups = []
        self.registered_parser_groups = []
        self.registered_destination_groups = []

    def set_global_config(self, global_config):
        self.global_config = global_config
        self.filemanager = self.global_config['filemanager']
        self.syslog_ng = self.global_config['syslog_ng']
        self.syslog_ng_path = self.global_config['syslog_ng_path']

    def add_file_source(self, file_path, file_content=None, options=None):
        return self.add_source_group(driver_name="file", options=options, file_content=file_content, file_path=file_path)

    def add_file_destination(self, file_path, options=None):
        return self.add_destination_group(driver_name="file", options=options, file_path=file_path)

    def add_tcp_source(self, options=None):
        return self.add_source_group(driver_name="tcp", options=options)

    def add_tcp_destination(self, options=None):
        return self.add_destination_group(driver_name="tcp", options=options)

    def add_source_group(self, driver_name, file_path=None, options=None, file_content=None):
        return self.add_group(group_type="source", driver_name=driver_name, options=options, file_content=file_content, file_path=file_path)

    def add_destination_group(self, driver_name, file_path=None, options=None):
        return self.add_group(group_type="destination", driver_name=driver_name, options=options, file_path=file_path)

    def add_parser_group(self, driver_name, options=None, database_content=None):
        return self.add_group(group_type="parser", driver_name=driver_name, options=options, database_content=database_content)

    def add_group(self, group_type, driver_name, file_path=None, options=None, **kwargs):
        group_name = self.generate_unique_group_name(group_type, driver_name)
        self.register_group(group_type=group_type, group=group_name)
        self.create_empty_group(group_type, group_name, driver_name)
        generated_options = self.generate_options(options, driver_name, group_type, file_path=file_path, **kwargs)
        self.add_options_to_existing_driver(group_type, group_name, generated_options)
        return group_name

    def generate_options(self, options, driver_name, group_type, file_path, **kwargs):
        generated_options = []
        if not options:
            options = {}
        if (driver_name in ["file", "pipe", "program"]) and (not options) or (driver_name in ["file", "pipe", "program"]) and ("path" not in options):
            if not file_path:
                file_path = self.global_config['global_register'].get_uniq_filename(prefix="%s_%s" % (group_type, driver_name))
            # WORKAROUND: @@ -> will be replaced in config render time to "'"
            generated_options.append({"option_name": "file_path", "option_value": "@@%s@@" % file_path})
        elif (driver_name in ["add_contextual_data"]) and ("database" not in options):
            if not file_path:
                file_path = self.global_config['global_register'].get_uniq_filename(prefix="%s_%s" % (group_type, driver_name), extension="csv")
            if "database_content" in kwargs.keys():
                self.filemanager.create_file_with_content(file_path, kwargs['database_content'])
            # WORKAROUND: @@ -> will be replaced in config render time to "'"
            generated_options.append({"option_name": "database", "option_value": "@@%s@@" % file_path})
        for option_name, option_value in options.items():
            if option_value == "not-set":
                # skip adding option
                pass
            else:
                generated_options.append({"option_name": option_name, "option_value": option_value})
        return generated_options

    def add_options_to_existing_driver(self, group_type, group_name, options):
        group_node = self.find_group_node(group_type, group_name)

        for option in options:
            # NOTE: without option name, e.g: "/tmp/input.log"
            if option["option_name"] == "file_path":
                self.add_option_to_node(node="without_option_name", group_node=group_node, group_type=group_type,
                                        option_value=option['option_value'], option_name=option['option_name'])

            if (option['option_name'] == "ip") and (group_type == "destination"):
                self.add_option_to_node(node="without_option_name", group_node=group_node, group_type=group_type,
                                        option_value=option['option_value'], option_name=option['option_name'])

        for option in options:
            if (option['option_name'] == "file_path") or (option['option_name'] == "ip" and group_type == "destination"):
                pass
            else:
            # NOTE: with option name, e.g: keep_hostname(yes)
                self.add_option_to_node(node="with_option_name", group_node=group_node, group_type=group_type, option_value=option['option_value'], option_name=option['option_name'])

    def is_options_already_added(self, group_type, group_node):
        if group_type == "parser":
            return len(group_node['driver_options'])
        elif group_type in ['source', 'destination']:
            return len(group_node['driver_options'][0]['non_tls_options'])
        else:
            # NOTE: Complete this section with new group_type
            import sys
            sys.exit(1)

    def add_option_to_node(self, node, group_node, group_type, option_value, option_name):
        found_node = False
        if group_type == "parser":
            if self.is_options_already_added(group_type, group_node) > 0:
                for existing_node in group_node['driver_options']:
                    if node in existing_node:
                        existing_node[node].append(
                            {"option_name": option_name, "option_value": option_value}
                        )
                        found_node = True
                if not found_node:
                    group_node['driver_options'].append(
                        {node: [{"option_name": option_name, "option_value": option_value}]}
                    )
            else:
                group_node['driver_options'].append(
                    {node: [{"option_name": option_name, "option_value": option_value}]}
                )

        elif group_type in ['source', 'destination']:
            if self.is_options_already_added(group_type, group_node) > 0:
                for existing_node in group_node['driver_options'][0]['non_tls_options']:
                    if node in existing_node:
                        existing_node[node].append(
                            {"option_name": option_name, "option_value": option_value}
                        )
                        found_node = True
                if not found_node:
                    group_node['driver_options'][0]['non_tls_options'].append(
                        {node: [{"option_name": option_name, "option_value": option_value}]}
                    )
            else:
                group_node['driver_options'][0]['non_tls_options'].append(
                    {node: [{"option_name": option_name, "option_value": option_value}]}
                )

        else:
            # NOTE: Complete this section with new group_type
            import sys
            sys.exit(1)

    def find_group_node(self, group_type, group_name):
        for group in self.config["%s_groups" % group_type]:
            if group["%s_group_name" % group_type] == group_name:
                return group["%s_drivers" % group_type][0]

    def create_empty_group(self, group_type, group_name, driver_name):
        if group_type in ['source', 'destination']:
            self.config["%s_groups" % group_type].append({
                "%s_group_name" % group_type: group_name,
                "%s_drivers" % group_type: [
                    {"%s_driver_name" % group_type: driver_name, "driver_options": [{"non_tls_options": []}]}]
            })
        elif group_type == "parser":
            self.config["%s_groups" % group_type].append({
                "%s_group_name" % group_type: group_name,
                "%s_drivers" % group_type: [
                    {"%s_driver_name" % group_type: driver_name, "driver_options": []}]
            })

    def generate_unique_group_name(self, group_type, driver_name):
        return "%s_%s_%s" % (group_type, driver_name, binascii.b2a_hex(os.urandom(5)).decode('ascii'))

    def generate_unique_file_path(self, group_type, driver_name, extension="txt"):
        if driver_name == "program":
            return "/tmp/%s_%s_%s.sh" % (
                group_type, driver_name, binascii.b2a_hex(os.urandom(5)).decode('ascii'))
        else:
            return "/tmp/%s_%s_%s.%s" % (
                group_type, driver_name, binascii.b2a_hex(os.urandom(5)).decode('ascii'), extension)

    def connect_drivers_in_logpath(self, sources, destinations, parsers=None):
        if not parsers:
            self.config["logpath"].append({
                "sources": [sources[0]],
                "destinations": [destinations[0]]
            })
        else:
            self.config["logpath"].append({
                "sources": [sources[0]],
                "parsers": [parsers[0]],
                "destinations": [destinations[0]]
            })

    def get_connected_config_elements(self):
        self.connected_config_elements = []
        for logpath in self.config['logpath']:
            source_file_path = None
            destination_file_path = None
            source_group_name = logpath['sources'][0]
            source_driver_name = source_group_name.split("_")[1]
            destination_group_name = logpath['destinations'][0]
            destination_driver_name = destination_group_name.split("_")[1]
            if source_driver_name in ['file', 'pipe', 'program']:
                source_file_path = self.get_file_path_for_group_name(group_name=source_group_name, group_type="source")
            if destination_driver_name in ['file', 'pipe', 'program']:
                destination_file_path = self.get_file_path_for_group_name(group_name=destination_group_name, group_type="destination")
            else:
                pass
            self.connected_config_elements.append({
                "source_group_name": source_group_name,
                "source_driver_name": source_driver_name,
                "source_file_path": source_file_path,
                "destination_group_name": destination_group_name,
                "destination_driver_name": destination_driver_name,
                "destination_file_path": destination_file_path
            })
        return self.connected_config_elements

    def render_config(self, logpath_management="auto", logpath=None):
        # custom_logpath = [[src_group1, filter_group1, dest_group1], [src_group2, filter_group2, dest_group2]]
        # config.render_config(logpath_management="custom", logpath=custom_logpath)
        self.add_internal_source_to_config()
        config_template = "%s/config_template.pystache" % os.path.dirname(os.path.abspath(__file__))
        config_path = self.syslog_ng_path.get_syslog_ng_config_path()
        if logpath_management == "auto":
            self.connect_drivers_in_logpath(sources=self.registered_source_groups, parsers=self.registered_parser_groups, destinations=self.registered_destination_groups)
        renderer = pystache.Renderer()
        with open(config_path, mode='wb') as config_file:
            config_file.write(bytes(renderer.render_path(config_template, self.config), 'utf-8'))
        with open(config_path, mode='r') as config_file:
            contents = config_file.read()
            replaced_contents = contents.replace("@@", "'")
        self.filemanager.create_file_with_content(config_path, replaced_contents)

    def add_internal_source_to_config(self):
        self.added_internal_source = True
        s_internal = self.add_source_group(driver_name="internal")
        d_file = self.add_destination_group(driver_name="file")
        self.connect_drivers_in_logpath(sources=[s_internal], destinations=[d_file])
        # raw_config = self.global_config['config'].get_config()
        # self.global_config['config'].render_config(raw_config, self.global_config['syslog_ng'].get_syslog_ng_config_path())
        # self.internal_log_output_path = self.global_config['config'].get_internal_log_output_path()
        # self.global_config['config'].get_connected_config_elements()

    def get_internal_log_output_path(self):
        found_internal_source_driver = False
        source_group_name = None
        destination_group_name_for_internal_source = None
        for source_group in self.config['source_groups']:
            for source_driver in source_group['source_drivers']:
                if source_driver['source_driver_name'] == 'internal':
                    source_group_name = source_group['source_group_name']
                    found_internal_source_driver = True
        if not found_internal_source_driver:
            logging.error("Missing internal source driver from config")
            sys.exit(1)
        for logpath_element in self.config['logpath']:
            if source_group_name in logpath_element['sources']:
                destination_group_name_for_internal_source = logpath_element['destinations'][0]
        for destination_group in self.config['destination_groups']:
            if destination_group['destination_group_name'] == destination_group_name_for_internal_source:
                return destination_group['destination_drivers'][0]['driver_options'][0]['non_tls_options'][0][
                    'without_option_name'][0]['option_value'].replace("@@", "")

    def add_global_options(self, options):
        for option_name, option_value in options.items():
            self.config["global_options"].append({
                "option_name": option_name,
                "option_value": option_value,
            })

    def get_config(self):
        return self.config

    def get_file_path_for_group_name(self, group_name, group_type):
        for group in self.config["%s_groups" % group_type]:
            if group["%s_group_name" % group_type] == group_name:
                return group["%s_drivers" % group_type][0]["driver_options"][0]["non_tls_options"][0]["without_option_name"][0]["option_value"].replace("@@", '"')

    def from_raw(self, config):
        config_path = self.syslog_ng_path.get_syslog_ng_config_path()
        self.filemanager.create_file_with_content(config_path, config)

    def register_group(self, group_type, group):
        if group_type == "source":
            self.registered_source_groups.append(group)
        elif group_type == "parser":
            self.registered_parser_groups.append(group)
        elif group_type == "destination":
            self.registered_destination_groups.append(group)