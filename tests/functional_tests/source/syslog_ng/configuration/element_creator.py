class SyslogNgConfigElementCreator(object):
    def __init__(self, testdb_logger, syslog_ng_config, config_connector, config_common, driver_data_provider):
        self.log_writer = testdb_logger.set_logger("SyslogNgConfigElementCreator")
        self.syslog_ng_config = syslog_ng_config
        self.config_connector = config_connector
        self.config_common = config_common
        self.driver_data_provider = driver_data_provider

        self.available_statements = ["source", "destination", "filter", "template", "parser", "rewrite"]

    def add_include(self, file_name):
        self.syslog_ng_config["include"].append(file_name)

    def add_module(self, module_name):
        self.syslog_ng_config["module"].append(module_name)

    def add_define(self, name, value):
        self.syslog_ng_config["define"].update({name: value})

    def add_global_options(self, options):
        self.syslog_ng_config["global_options"].update(options)

    # statements
    def create_empty_statement(self, statement_name):
        if statement_name not in self.available_statements:
            self.log_writer.error("Unknown statement: %s" % statement_name)
            assert False
        statement_id = self.config_common.generate_statement_id(statement_name)
        empty_statement = {
            statement_id: {},
        }
        return empty_statement, statement_id

    def create_source(self, driver_name, driver_options=None, use_mandatory_options=True):
        return self.create_statement(statement_name="source", driver_name=driver_name, driver_options=driver_options, use_mandatory_options=use_mandatory_options)

    def create_destination(self, driver_name, driver_options=None, use_mandatory_options=True):
        return self.create_statement(statement_name="destination", driver_name=driver_name, driver_options=driver_options, use_mandatory_options=use_mandatory_options)

    def create_statement(self, statement_name, driver_name, driver_options, use_mandatory_options):
        empty_statement, statement_id = self.create_empty_statement(statement_name)
        driver_properties, driver_id = self.create_driver_properties(statement_id=statement_id, driver_name=driver_name, driver_options=driver_options, use_mandatory_options=use_mandatory_options)

        statement = self.config_connector.connect_driver_with_statement(driver_properties=driver_properties, statement=empty_statement)
        self.config_connector.connect_statement_to_root_config(statement=statement, statement_name=statement_name)
        return statement_id, driver_id

    # drivers
    def create_driver_properties(self, statement_id, driver_name, driver_options, use_mandatory_options):
        driver_id = "%s_%s" % (statement_id, driver_name)

        if use_mandatory_options:
            if not driver_options:
                driver_options = {}
            mandatory_options = self.driver_data_provider.get_config_mandatory_options(driver_name=driver_name, driver_id=driver_id)
        else:
            mandatory_options = self.driver_data_provider.get_config_mandatory_options(driver_name=driver_name, defined_driver_options=driver_options)
        connection_mandatory_options = self.driver_data_provider.get_connection_mandatory_options(driver_name=driver_name, defined_driver_options=mandatory_options)
        merged_driver_options = {**driver_options, **mandatory_options}

        driver_properties = {
            driver_id: {
                "driver_name": driver_name,
                "driver_options": merged_driver_options,
                "config_mandatory_options": mandatory_options,
                "connection_mandatory_options": connection_mandatory_options,
                "writer_class": self.driver_data_provider.get_driver_writer(driver_name=driver_name),
                "listener_class": self.driver_data_provider.get_driver_listener(driver_name=driver_name)
            },
        }
        return driver_properties, driver_id

    # multi statements + logpath
    def create_connected_source_with_destination(self, source_driver_name, source_options, destination_driver_name, destination_options):
        src_statement_id, src_driver_id = self.create_source(source_driver_name, source_options)
        dst_statement_id, dst_driver_id = self.create_destination(destination_driver_name, destination_options)
        self.config_connector.connect_statements_in_logpath(source_statements=[src_statement_id], destination_statements=[dst_statement_id])
        return src_statement_id, src_driver_id, dst_statement_id, dst_driver_id

    def create_multiple_connected_source_with_destination(self, source_driver_names, source_options, destination_driver_names, destination_options):
        for source_driver_name, destination_driver_name in zip(source_driver_names, destination_driver_names):
            src_statement_id, src_driver_id, dst_statement_id, dst_driver_id = self.create_connected_source_with_destination(source_driver_name=source_driver_name, source_options=source_options, destination_driver_name=destination_driver_name, destination_options=destination_options)

    def add_internal_source_to_config(self, file_register):
        src_statement_id, src_driver_id = self.create_source(driver_name="internal")
        internal_log_path = file_register.get_registered_file_path(prefix="destination_internal_log_path")
        dst_statement_id, dst_driver_id = self.create_destination(driver_name="file", driver_options={"file_path": internal_log_path}, use_mandatory_options=False)
        self.config_connector.connect_statements_in_logpath(source_statements=[src_statement_id], destination_statements=[dst_statement_id])
        return src_statement_id, src_driver_id, dst_statement_id, dst_driver_id
