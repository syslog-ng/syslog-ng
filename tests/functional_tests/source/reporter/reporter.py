class Reporter(object):
    def __init__(self, testdb_logger, syslog_ng_config_interface, message_interface, driver_data_provider):
        self.log_writer = testdb_logger.set_logger("Reporter")
        self.syslog_ng_config_interface = syslog_ng_config_interface
        self.message_interface = message_interface
        self.driver_data_provider = driver_data_provider

        self.input_message_collector = {}
        self.expected_message_collector = {}
        self.actual_message_collector = {}

    def compare_messages(self, expected_message, actual_message):
        assert expected_message == actual_message

    def compare_collected_messages(self):
        self.log_writer.info("STEP Comparing collected messages")
        if not self.actual_message_collector:
            self.log_writer.error("There was no outgoing messages from syslog-ng, check senders and listeners")
            assert False
        final_result = self.check_if_messages_arrived_from_every_source()
        for actual_destination_id in self.actual_message_collector:
            result = self.check_expected_number_of_messages(key=actual_destination_id)
            result2 = self.check_expected_message_content(key=actual_destination_id)
            if (not result) or (not result2):
                final_result = False

        assert final_result is True

    def save_expected_message(self, defined_message_parts=None, counter=1, expected_raw_message=None):
        destination_statement_properties = self.syslog_ng_config_interface.get_destination_statement_properties()
        for destination_statement_id, destination_driver in destination_statement_properties.items():
            for destination_driver_id, destination_driver_properties in destination_driver.items():
                if "internal" not in destination_driver_properties['connection_mandatory_options']:
                    driver_name = destination_driver_properties['driver_name']

                    if expected_raw_message:
                        self.save_message(collector="expected", messages=expected_raw_message, driver_id=destination_driver_id)
                    else:
                        expected_messages = self.message_interface.create_message_for_destination_driver(driver_name=driver_name, defined_message_parts=defined_message_parts, counter=counter)
                        self.save_message(collector="expected", messages=expected_messages, driver_id=destination_driver_id)

    def save_message(self, collector, messages, driver_id):
        if isinstance(messages, str):
            messages = [messages]
        if collector == "input":
            self.input_message_collector.update({driver_id: messages})
        elif collector == "expected":
            self.expected_message_collector.update({driver_id: messages})
        elif collector == "actual":
            self.actual_message_collector.update({driver_id: messages})

    def dump_collectors(self):
        self.log_writer.info("INPUT message: %s" % self.input_message_collector)
        self.log_writer.info("EXPECTED message: %s" % self.expected_message_collector)
        self.log_writer.info("ACTUAL message: %s" % self.actual_message_collector)

    def check_if_messages_arrived_from_every_source(self):
        if self.actual_message_collector.keys() != self.expected_message_collector.keys():
            for actual_dict_key_element in self.actual_message_collector:
                if actual_dict_key_element not in self.expected_message_collector.keys():
                    self.log_writer.error("We did not expect message for [%s], but it arrived." % actual_dict_key_element)
            for expected_dict_key_element in self.expected_message_collector:
                if expected_dict_key_element not in self.actual_message_collector.keys():
                    self.log_writer.error("We expect message for [%s], but it not arrived." % expected_dict_key_element)
            return False
        return True

    def check_expected_number_of_messages(self, key):
        if len(self.actual_message_collector[key]) != len(self.expected_message_collector[key]):
            self.log_writer.error("Number of messages did not equals with expected, driver_id:[%s]" % key)
            self.log_writer.error("Expected number of messages: [%s]" % len(self.expected_message_collector[key]))
            self.log_writer.error("Arrived number of messages: [%s]" % len(self.actual_message_collector[key]))
            return False
        self.log_writer.info("SUBSTEP: Actual and expected number of messages are equals")
        return True

    def check_expected_message_content(self, key):
        if self.actual_message_collector[key] != self.expected_message_collector[key]:
            self.log_writer.error("Messages did not equals with expected, driver_id:[%s]" % key)
            self.log_writer.error("Expected messages: [%s]" % self.expected_message_collector[key])
            self.log_writer.error("Arrived messages: [%s]" % self.actual_message_collector[key])
            return False
        self.log_writer.info("SUBSTEP: Actual and expected messages are equals")
        return True

    def is_message_substring_in_list_of_messages(self, message_substring, list_of_messages):
        for message in list_of_messages:
            if message_substring in message:
                return True
        return False
