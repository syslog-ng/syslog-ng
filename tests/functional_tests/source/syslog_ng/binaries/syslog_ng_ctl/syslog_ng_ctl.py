from source.testdb.common.common import wait_till_function_not_false, wait_till_function_not_true


class SyslogNgCtl(object):
    def __init__(self, testdb_logger, executor, driver_data_provider, syslog_ng_config_interface, syslog_ng_path_database):
        self.testdb_logger = testdb_logger
        self.log_writer = testdb_logger.set_logger("SyslogNgCtl")
        self.executor = executor
        self.driver_data_provider = driver_data_provider
        self.syslog_ng_config_interface = syslog_ng_config_interface
        self.syslog_ng_path_database = syslog_ng_path_database

        self.first_matched_stats = None
        self.first_matched_query = None

    def stats(self, reset=False):
        stats_command = "%s stats --control=%s" % (self.syslog_ng_path_database.get_syslog_ng_control_tool_path(), self.syslog_ng_path_database.get_control_socket_path())
        if reset:
            stats_command += " --reset"
        exit_code, stdout, stderr = self.executor.execute_command(stats_command)
        return exit_code, stdout, stderr

    def query(self, pattern="*", query_get=True, query_list=False, query_sum=False, reset=False):
        query_command = "%s query --control=%s" % (self.syslog_ng_path_database.get_syslog_ng_control_tool_path(), self.syslog_ng_path_database.get_control_socket_path())
        if query_get:
            query_command += " get"
        elif query_sum:
            query_command += " get --sum"
        elif query_list:
            query_command += " list"
        elif reset:
            query_command += " get --reset"
        query_command += " '%s' " % pattern

        exit_code, stdout, stderr = self.executor.execute_command(query_command)
        return exit_code, stdout, stderr

    def stop(self):
        stop_command = "%s stop --control=%s" % (self.syslog_ng_path_database.get_syslog_ng_control_tool_path(), self.syslog_ng_path_database.get_control_socket_path())
        exit_code, stdout, stderr = self.executor.execute_command(stop_command)
        return exit_code, stdout, stderr

    def reload(self):
        reload_command = "%s reload --control=%s" % (self.syslog_ng_path_database.get_syslog_ng_control_tool_path(), self.syslog_ng_path_database.get_control_socket_path())
        exit_code, stdout, stderr = self.executor.execute_command(reload_command)
        return exit_code, stdout, stderr

    def reopen(self):
        reopen_command = "%s reopen --control=%s" % (self.syslog_ng_path_database.get_syslog_ng_control_tool_path(), self.syslog_ng_path_database.get_control_socket_path())
        exit_code, stdout, stderr = self.executor.execute_command(reopen_command)
        return exit_code, stdout, stderr

    def is_control_socket_alive(self):
        exit_code, stdout, stderr = self.stats()
        return exit_code == 0

    def wait_for_control_socket_start(self):
        return wait_till_function_not_true(self.is_control_socket_alive)

    def wait_for_control_socket_stop(self):
        return wait_till_function_not_false(self.is_control_socket_alive)

    def check_counters_for_sources(self, message_counter):
        source_statement_properties = self.syslog_ng_config_interface.get_source_statement_properties()
        if source_statement_properties != {}:
            for source_statement_id, source_driver in source_statement_properties.items():
                for source_driver_id, source_driver_properties in source_driver.items():
                    if source_driver_properties['driver_name'] != "internal":
                        driver_name = source_driver_properties['driver_name']
                        connection_mandatory_options = source_driver_properties['connection_mandatory_options']
                        assert self.wait_for_query_counter(component="src.%s" % driver_name, config_id=source_statement_id, instance=connection_mandatory_options, counter_type="processed", message_counter=message_counter) is True
                        assert self.wait_for_stats_counter(component="src.%s" % driver_name, config_id=source_statement_id, instance=connection_mandatory_options, state_type='a', counter_type="processed", message_counter=message_counter) is True
        else:
            self.log_writer.info("Skip checking source counters. (Maybe raw config was used)")

    def check_counters_for_destinations(self, message_counter):
        destination_statement_properties = self.syslog_ng_config_interface.get_destination_statement_properties()
        if destination_statement_properties != {}:
            for destination_statement_id, destination_driver in destination_statement_properties.items():
                for destination_driver_id, destination_driver_properties in destination_driver.items():
                    if "internal" not in destination_driver_properties['connection_mandatory_options']:
                        driver_name = destination_driver_properties['driver_name']
                        connection_mandatory_options = destination_driver_properties['connection_mandatory_options']
                        assert self.wait_for_query_counter(component="dst.%s" % driver_name, config_id=destination_statement_id, instance=connection_mandatory_options, counter_type="processed", message_counter=message_counter) is True
                        assert self.wait_for_query_counter(component="dst.%s" % driver_name, config_id=destination_statement_id, instance=connection_mandatory_options, counter_type="written", message_counter=message_counter) is True
                        assert self.wait_for_query_counter(component="dst.%s" % driver_name, config_id=destination_statement_id, instance=connection_mandatory_options, counter_type="dropped", message_counter=0) is True
                        assert self.wait_for_query_counter(component="dst.%s" % driver_name, config_id=destination_statement_id, instance=connection_mandatory_options, counter_type="queued", message_counter=0) is True
                        assert self.wait_for_query_counter(component="dst.%s" % driver_name, config_id=destination_statement_id, instance=connection_mandatory_options, counter_type="memory_usage", message_counter=0) is True
                        assert self.wait_for_stats_counter(component="dst.%s" % driver_name, config_id=destination_statement_id, instance=connection_mandatory_options, state_type="a", counter_type="processed", message_counter=message_counter) is True
                        assert self.wait_for_stats_counter(component="dst.%s" % driver_name, config_id=destination_statement_id, instance=connection_mandatory_options, state_type="a", counter_type="written", message_counter=message_counter) is True
                        assert self.wait_for_stats_counter(component="dst.%s" % driver_name, config_id=destination_statement_id, instance=connection_mandatory_options, state_type="a", counter_type="dropped", message_counter=0) is True
                        assert self.wait_for_stats_counter(component="dst.%s" % driver_name, config_id=destination_statement_id, instance=connection_mandatory_options, state_type="a", counter_type="queued", message_counter=0) is True
                        assert self.wait_for_stats_counter(component="dst.%s" % driver_name, config_id=destination_statement_id, instance=connection_mandatory_options, state_type="a", counter_type="memory_usage", message_counter=0) is True
        else:
            self.log_writer.info("Skip checking destination counters. (Maybe raw config was used)")

    def check_counters(self, message_counter=1):
        self.log_writer.info("STEP Checking statistical counters")
        self.check_counters_for_sources(message_counter=message_counter)
        self.check_counters_for_destinations(message_counter=message_counter)

    def wait_for_query_counter(self, component, config_id, instance, counter_type="processed", message_counter=1):
        query_line = "%s.%s#0.%s.%s=%s" % (
            component,
            config_id,
            instance,
            counter_type,
            message_counter)

        if self.first_matched_query and (query_line in self.first_matched_query):
            result_of_query_in_query = True
        else:
            result_of_query_in_query = wait_till_function_not_true(func=self.is_line_in_query, arg1=query_line, monitoring_time=1)

        self.testdb_logger.write_message_based_on_value(logsource=self.log_writer, message="Found stat line: [%s] in query" % query_line, value=result_of_query_in_query)
        return result_of_query_in_query

    def wait_for_stats_counter(self, component, config_id, instance, state_type="a", counter_type="processed", message_counter=1):
        stats_line = "%s;%s#0;%s;%s;%s;%s" % (
            component,
            config_id,
            instance,
            state_type,
            counter_type,
            message_counter)

        if self.first_matched_stats and (stats_line in self.first_matched_stats):
            result_of_stats_in_stats = True
        else:
            result_of_stats_in_stats = wait_till_function_not_true(func=self.is_line_in_stats, arg1=stats_line, monitoring_time=1)

        self.testdb_logger.write_message_based_on_value(logsource=self.log_writer, message="Found stat line: [%s] in stats" % stats_line, value=result_of_stats_in_stats)
        return result_of_stats_in_stats

    def is_line_in_stats(self, stats_line):
        statistics = str(self.stats()[1])
        result = "%s\n" % stats_line in statistics
        if result:
            self.first_matched_stats = statistics
        return result

    def is_line_in_query(self, query_line):
        statistics = str(self.query()[1])
        result = "%s\n" % query_line in statistics
        if result:
            self.first_matched_query = statistics
        return result
