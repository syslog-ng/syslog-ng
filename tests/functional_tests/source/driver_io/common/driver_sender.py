class DriverSender(object):
    def __init__(self, testdb_logger, message_interface):
        self.log_writer = testdb_logger.set_logger("DriverSender")
        self.message_interface = message_interface

    def write_content_to_driver(self, source_driver_properties, source_driver_id, defined_message_parts, counter, message_queue):
        driver_name = source_driver_properties['driver_name']
        connection_mandatory_options = source_driver_properties['connection_mandatory_options']

        input_messages = self.message_interface.create_message_for_source_driver(driver_name=driver_name, defined_message_parts=defined_message_parts, counter=counter)
        source_driver_properties['writer_class'].write_content(connection_mandatory_options, content=input_messages, driver_name=driver_name)
        message_queue.append({"driver_id": source_driver_id, "messages": input_messages})
