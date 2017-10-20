from multiprocessing import Process, Manager
from source.driver_io.common.driver_sender import DriverSender


class ThreadedSender(object):
    def __init__(self, testdb_logger, syslog_ng_config_interface, message_interface, testdb_reporter=None):
        self.log_writer = testdb_logger.set_logger("ThreadedSender")
        self.syslog_ng_config_interface = syslog_ng_config_interface
        self.testdb_reporter = testdb_reporter

        self.driver_sender = DriverSender(testdb_logger=testdb_logger, message_interface=message_interface)
        self.message_queue = Manager().list()
        self.process_pool = []

    def start_senders(self, defined_message_parts=None, counter=1):
        self.log_writer.info("STEP Starting threded senders")
        source_statement_properties = self.syslog_ng_config_interface.get_source_statement_properties()
        for source_statement_id, source_driver in source_statement_properties.items():
            for source_driver_id, source_driver_properties in source_driver.items():
                if source_driver_properties['driver_name'] != "internal":
                    process = Process(
                        target=self.driver_sender.write_content_to_driver,
                        args=(source_driver_properties, source_driver_id,
                              defined_message_parts, counter, self.message_queue)
                    )
                    process.start()
                    self.process_pool.append(process)
        self.close_senders()

    def close_senders(self):
        self.log_writer.info("STEP Stopping threded senders")
        if len(self.process_pool) is not 0:
            for driver_sender_process in self.process_pool:
                driver_sender_process.join()
                assert not driver_sender_process.exitcode
        self.process_message_queue()

    def process_message_queue(self):
        for message_queue_index, message_queue_content_for_driver in enumerate(list(self.message_queue)):
            driver_id = message_queue_content_for_driver['driver_id']
            messages = message_queue_content_for_driver['messages']
            self.testdb_reporter.save_message(collector="input", messages=messages, driver_id=driver_id)
