class DriverListener(object):
    def __init__(self, testdb_logger):
        self.log_writer = testdb_logger.set_logger("DriverListener")

    def wait_for_content_on_driver(self, destination_driver_properties, destination_driver_id, message_queue, message_counter=1):
        driver_name = destination_driver_properties['driver_name']
        connection_mandatory_options = destination_driver_properties['connection_mandatory_options']

        actual_received_messages = destination_driver_properties['listener_class'].wait_for_content(connection_mandatory_options, driver_name=driver_name, message_counter=message_counter)
        message_queue.append({"driver_id": destination_driver_id, "messages": actual_received_messages})
