from src.send_receive_logs.log_generator.log_generator import MessageGenerator
from src.work_with_sockets import network_based_processor
from src.options_and_drivers.driver_data_provider import DriverDataProvider

class LogSender(object):
    def __init__(self):
        self.global_config = None

    def set_global_config(self, global_config):
        self.global_config = global_config

    def send_default_messages_to_source_side(self, counter=1):
        self.send_messages_to_source_side(counter)

    def send_custom_messages_to_source_side(self, counter=1, **kwargs):
        self.send_messages_to_source_side(counter, **kwargs)

    def send_messages_to_source_side(self, counter, **kwargs):
        customized_message_parts = self.create_customized_message_parts(**kwargs)
        message_generator = MessageGenerator(customized_message_parts, counter)

        for connected_config_element in self.global_config['config'].get_connected_config_elements():
            source_driver_name = connected_config_element['source_driver_name']
            source_group_name = connected_config_element['source_group_name']
            destination_driver_name = connected_config_element['destination_driver_name']
            destination_group_name = connected_config_element['destination_group_name']

            if source_driver_name != "internal":
                input_messages = message_generator.generate_input_messages(source_driver_name, destination_driver_name)
                self.global_config['testrun_reporter'].save_input_messages(source_group_name, input_messages)
                self.write_messages_to_source(connected_config_element, input_messages)
        self.global_config['syslog_ng'].reload()

    def create_customized_message_parts(self, **kwargs):
        customized_message_parts = {}
        for field_name, field_value in kwargs.items():
            customized_message_parts[field_name] = field_value
        return customized_message_parts

    def write_messages_to_source(self, connected_config_element, input_messages):
        driver_data_provider = DriverDataProvider()
        for input_message in input_messages:
            if connected_config_element['source_driver_name'] in driver_data_provider.get_all_drivers_for_working_type("file"):
                self.global_config['file_based_processor'].write_message_to_file(connected_config_element['source_file_path'], input_message,
                                                                                 connected_config_element['source_driver_name'])
            elif connected_config_element['source_driver_name'] in driver_data_provider.get_all_drivers_for_working_type("socket-stream"):
                if "unix" in connected_config_element['source_driver_name']:
                    network_based_processor.send_message_to_stream_socket(connected_config_element['source_driver_name'], input_message,
                                                                          connected_config_element['source_file_path'])
                else:
                    network_based_processor.send_message_to_stream_socket(connected_config_element['source_driver_name'], input_message)
            elif connected_config_element['source_driver_name'] in driver_data_provider.get_all_drivers_for_working_type("socket-dgram"):
                if "unix" in connected_config_element['source_driver_name']:
                    network_based_processor.send_message_to_dgram_socket(connected_config_element['source_driver_name'], input_message,
                                                                         connected_config_element['source_file_path'])
                else:
                    network_based_processor.send_message_to_dgram_socket(connected_config_element['source_driver_name'], input_message)
            else:
                print("unknown driver: %s" % connected_config_element['driver_name'])
                assert False
