from source.testdb.initializer.setup_classes import SetupClasses


class SetupTestcase(SetupClasses):
    def __init__(self, testcase_context, topology="server"):
        SetupClasses.__init__(self, testcase_context=testcase_context, topology=topology)
        self.testcase_context = testcase_context
        self.topology = topology
        self.file_common.save_testcase_file(testcase_name=self.testcase_name, destination_path=self.testdb_path_database.testcase_working_dir)

    def run(self, expected_raw_message=None, message_counter=1):
        if not self.source_driver:
            self.log_writer.error("Missing syslog-ng configuration. Please configure syslog-ng with one of the methods")
            assert False
        self.start_listeners(message_counter=message_counter)
        if self.source_driver == 'file':
            self.send_message(message_counter=message_counter)
        self.start_syslog_ng()
        if self.source_driver != "file":
            self.send_message(message_counter=message_counter)
        self.stop_listeners()
        self.compare_results(message_counter=message_counter, expected_raw_message=expected_raw_message)

    def build_config_with_single_drivers(self, source_driver="file", destination_driver="file", global_options=None, source_options=None, destination_options=None):
        if global_options is None:
            global_options = {"stats_level": 3, "keep_hostname": "yes"}
        self.log_writer.info("STEP Configuring syslog-ng with single drivers")
        self.source_driver = source_driver
        self.syslog_ng_config_interface.add_global_options(global_options)
        self.syslog_ng_config_interface.create_connected_source_with_destination(
            source_driver_name=source_driver,
            source_options=source_options,
            destination_driver_name=destination_driver,
            destination_options=destination_options
        )
        self.syslog_ng_config_interface.generate_config()

    def build_config_with_destination_drivers(self, destination_drivers="all", source_drivers="file", excluded_drivers=None):
        self.log_writer.info("STEP Configuring syslog-ng with destination drivers")
        if destination_drivers == "all":
            destination_drivers = self.driver_data_provider.get_all_destination_drivers()
        if excluded_drivers:
            for driver_name in excluded_drivers:
                destination_drivers.remove(driver_name)
        self.source_driver = source_drivers
        self.syslog_ng_config_interface.add_global_options({"stats_level": 3, "keep_hostname": "yes"})
        self.syslog_ng_config_interface.create_multiple_connected_source_with_destination(
            source_driver_names=[source_drivers]*len(destination_drivers),
            destination_driver_names=destination_drivers,
            source_options=None,
            destination_options=None
        )
        self.syslog_ng_config_interface.generate_config()

    def build_config_with_source_drivers(self, source_drivers="all", destination_drivers="file"):
        self.log_writer.info("STEP Configuring syslog-ng with source drivers")
        if source_drivers == "all":
            source_drivers = self.driver_data_provider.get_all_source_drivers()
        self.source_driver = "all"
        self.syslog_ng_config_interface.add_global_options({"stats_level": 3})
        self.syslog_ng_config_interface.create_multiple_connected_source_with_destination(
            source_driver_names=source_drivers,
            destination_driver_names=[destination_drivers]*len(source_drivers),
            source_options=None,
            destination_options=None
        )
        self.syslog_ng_config_interface.generate_config()

    def start_listeners(self, message_counter):
        self.threaded_listener.start_listeners(message_counter=message_counter)

    def start_syslog_ng(self, expected_run=True, external_tool=None):
        self.syslog_ng.start(expected_run=expected_run, external_tool=external_tool)

    def send_message(self, message_counter=1, defined_message_parts=None):
        self.threaded_sender.start_senders(defined_message_parts=defined_message_parts, counter=message_counter)

    def stop_listeners(self):
        self.threaded_listener.close_listeners()

    def compare_results(self, defined_message_parts=None, message_counter=1, expected_raw_message=None):
        self.testdb_reporter.save_expected_message(defined_message_parts=defined_message_parts, counter=message_counter, expected_raw_message=expected_raw_message)
        self.testdb_reporter.compare_collected_messages()
        self.syslog_ng_ctl.check_counters(message_counter=message_counter)
