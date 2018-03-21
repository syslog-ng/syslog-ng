from source.testdb.common.common import get_unique_id


class SyslogNgConfigCommon(object):
    def __init__(self, testdb_logger, syslog_ng_config):
        self.log_writer = testdb_logger.set_logger("SyslogNgConfigCommon")
        self.syslog_ng_config = syslog_ng_config

    def get_logpath_node_where_statment_exist(self, statement_id):
        for logpath_node in self.syslog_ng_config['logpaths']:
            for statement, statement_ids in logpath_node.items():
                if statement_id in statement_ids:
                    return logpath_node

    def get_connected_destination_statements_where_statement_exist(self, statement_id):
        return self.get_logpath_node_where_statment_exist(statement_id=statement_id)['destination_statements']

    def get_connected_source_statements_where_statement_exist(self, statement_id):
        return self.get_logpath_node_where_statment_exist(statement_id=statement_id)['source_statements']

    @staticmethod
    def generate_statement_id(statement_name):
        return "%s_%s" % (statement_name, get_unique_id())

    def get_source_statement_properties(self):
        return self.syslog_ng_config['source_statements']

    def get_destination_statement_properties(self):
        return self.syslog_ng_config['destination_statements']
