class SyslogNgConfigElementConnector(object):
    def __init__(self, testdb_logger, syslog_ng_config):
        self.log_writer = testdb_logger.set_logger("SyslogNgConfigElementConnector")
        self.syslog_ng_config = syslog_ng_config

    def connect_driver_with_statement(self, driver_properties, statement, statement_name=None):
        if statement_name:
            return self.connect_driver_with_statement_name(driver=driver_properties, statement=statement, statement_name=statement_name)
        return self.connect_driver_with_statement_object(driver=driver_properties, statement=statement)

    @staticmethod
    def connect_driver_with_statement_object(driver, statement):
        if len(statement.keys()) != 1:
            self.log_writer.error("Can not add driver to multiple statements")
            assert False
        for key in statement.keys():
            statement[key] = driver
        return statement

    @staticmethod
    def connect_driver_with_statement_name(driver, statement, statement_name):
        statement[statement_name] = driver
        return statement

    def connect_statement_to_root_config(self, statement, statement_name):
        self.syslog_ng_config["%s_statements" % statement_name].update(statement)

    def connect_statements_in_logpath(self, source_statements, destination_statements):
        self.syslog_ng_config['logpaths'].append({
            "source_statements": source_statements,
            "destination_statements": destination_statements
        })
