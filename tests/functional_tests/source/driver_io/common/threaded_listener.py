from multiprocessing import Process, Manager
from source.driver_io.common.driver_listener import DriverListener


class ThreadedListener(object):
    def __init__(self, testdb_logger, syslog_ng_config_interface, testdb_reporter, driver_data_provider):
        self.log_writer = testdb_logger.set_logger("ThreadedListener")
        self.syslog_ng_config_interface = syslog_ng_config_interface
        self.testdb_reporter = testdb_reporter
        self.driver_data_provider = driver_data_provider

        self.driver_listener = DriverListener(testdb_logger=testdb_logger)
        self.message_queue = Manager().list()
        self.process_pool = []

    def start_listeners(self, message_counter):
        self.log_writer.info("STEP Starting threded listeners")
        destination_statement_properties = self.syslog_ng_config_interface.get_destination_statement_properties()
        for destination_statement_id, destination_driver in destination_statement_properties.items():
            for destination_driver_id, destination_driver_properties in destination_driver.items():
                if "internal" not in destination_driver_properties['connection_mandatory_options']:
                    process = Process(
                        target=self.driver_listener.wait_for_content_on_driver,
                        args=(destination_driver_properties, destination_driver_id, self.message_queue, message_counter)
                    )
                    process.start()
                    self.process_pool.append(process)

    def close_listeners(self):
        self.log_writer.info("STEP Stopping threded listeners")
        if len(self.process_pool) is not 0:
            for driver_listener_process in self.process_pool:
                driver_listener_process.join()
                assert not driver_listener_process.exitcode
        self.process_message_queue()

    def process_message_queue(self):
        for message_queue_index, message_queue_content_for_driver in enumerate(list(self.message_queue)):
            driver_id = message_queue_content_for_driver['driver_id']
            messages = message_queue_content_for_driver['messages']
            self.testdb_reporter.save_message(collector="actual", messages=messages, driver_id=driver_id)
