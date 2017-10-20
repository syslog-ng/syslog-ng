from source.syslog_ng.configuration.common import SyslogNgConfigCommon
from source.syslog_ng.configuration.element_connector import SyslogNgConfigElementConnector
from source.syslog_ng.configuration.element_creator import SyslogNgConfigElementCreator
from source.syslog_ng.configuration.writer import SyslogNgConfigWriter


class SyslogNgConfigInterface(object):
    def __init__(self, testdb_logger, testdb_config_context, testdb_path_database, driver_data_provider, file_register, file_writer, topology):
        self.log_writer = testdb_logger.set_logger("SyslogNgConfigInterface")
        self.file_register = file_register
        self.file_writer = file_writer
        self.topology = topology

        self.statement_pattern = "statements"
        self.syslog_ng_config = {
            "version": testdb_config_context.syslog_ng_version,
            "include": ["scl.conf"],
            "module": [],
            "define": {},
            "channel": [],
            "block": [],
            "global_options": {},
            "source_%s" % self.statement_pattern: {},
            "parser_%s" % self.statement_pattern: [],
            "template_%s" % self.statement_pattern: [],
            "rewrite_%s" % self.statement_pattern: [],
            "filter_%s" % self.statement_pattern: [],
            "destination_%s" % self.statement_pattern: {},
            "logpaths": []
        }
        self.config_common = SyslogNgConfigCommon(testdb_logger=testdb_logger, syslog_ng_config=self.syslog_ng_config)
        self.config_element_connector = SyslogNgConfigElementConnector(testdb_logger=testdb_logger, syslog_ng_config=self.syslog_ng_config)
        self.config_element_creator = SyslogNgConfigElementCreator(
            testdb_logger=testdb_logger,
            syslog_ng_config=self.syslog_ng_config,
            config_connector=self.config_element_connector,
            config_common=self.config_common,
            driver_data_provider=driver_data_provider)
        self.config_writer = SyslogNgConfigWriter(testdb_logger=testdb_logger, syslog_ng_config=self.syslog_ng_config, file_register=self.file_register, driver_data_provider=driver_data_provider, testdb_path_database=testdb_path_database)

    # Interfaces for ConfigElementCreator
    def create_source(self, driver_name, driver_options=None, use_mandatory_options=True):
        return self.config_element_creator.create_source(driver_name=driver_name, driver_options=driver_options, use_mandatory_options=use_mandatory_options)

    def create_destination(self, driver_name, driver_options=None, use_mandatory_options=True):
        return self.config_element_creator.create_destination(driver_name=driver_name, driver_options=driver_options, use_mandatory_options=use_mandatory_options)

    def create_connected_source_with_destination(self, source_driver_name, destination_driver_name, source_options=None, destination_options=None):
        return self.config_element_creator.create_connected_source_with_destination(source_driver_name=source_driver_name, source_options=source_options, destination_driver_name=destination_driver_name, destination_options=destination_options)

    def create_multiple_connected_source_with_destination(self, source_driver_names, source_options, destination_driver_names, destination_options):
        return self.config_element_creator.create_multiple_connected_source_with_destination(source_driver_names=source_driver_names, source_options=source_options, destination_driver_names=destination_driver_names, destination_options=destination_options)

    def create_driver_properties(self, statement_id, driver_name, driver_options, use_mandatory_options):
        return self.config_element_creator.create_driver_properties(statement_id=statement_id, driver_name=driver_name, driver_options=driver_options, use_mandatory_options=use_mandatory_options)

    def add_internal_source_to_config(self):
        return self.config_element_creator.add_internal_source_to_config(file_register=self.file_register)

    def add_global_options(self, global_options):
        return self.config_element_creator.add_global_options(options=global_options)

    # Interfaces for ConfigCommon
    def get_source_statement_properties(self):
        return self.config_common.get_source_statement_properties()

    def get_destination_statement_properties(self):
        return self.config_common.get_destination_statement_properties()

    # Interface for ConfigWriter
    def write_config(self):
        return self.config_writer.write_config(file_writer=self.file_writer, topology=self.topology)

    def write_raw_config(self, raw_config):
        return self.config_writer.write_raw_config(raw_config=raw_config, file_writer=self.file_writer, topology=self.topology)

    # Interface for ConfigCommon
    def get_logpath_node_where_statment_exist(self, statement_id):
        return self.config_common.get_logpath_node_where_statment_exist(statement_id=statement_id)

    def get_connected_destination_statements_where_statement_exist(self, statement_id):
        return self.config_common.get_connected_destination_statements_where_statement_exist(statement_id=statement_id)

    def get_connected_source_statements_where_statement_exist(self, statement_id):
        return self.config_common.get_connected_source_statements_where_statement_exist(statement_id=statement_id)

    # High level functions
    def generate_config(self, use_internal_source=True):
        if use_internal_source:
            self.config_element_creator.add_internal_source_to_config(file_register=self.file_register)
        self.config_writer.write_config(file_writer=self.file_writer, topology=self.topology)

    def generate_raw_config(self, raw_config):
        self.config_writer.write_raw_config(raw_config=raw_config, file_writer=self.file_writer, topology=self.topology)
