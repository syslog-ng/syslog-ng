from src.send_receive_logs.log_generator.log_generator import MessageGenerator
from src.work_with_sockets.socket_listener import SocketListener
from src.options_and_drivers.driver_data_provider import DriverDataProvider

class LogReceiver(object):
    def __init__(self, ):
        self.global_config = None
        self.socket_listener = SocketListener()

    def set_global_config(self, global_config):
        self.global_config = global_config

    def collect_actual_output_messages_from_destination_side(self, counter=1, **kwargs):
        customized_message_parts = self.create_customized_message_parts(**kwargs)
        message_generator = MessageGenerator(customized_message_parts, counter)
        actual_output_messages = ""

        for connected_config_element in self.global_config['config'].get_connected_config_elements():
            source_driver_name = connected_config_element['source_driver_name']
            source_group_name = connected_config_element['source_group_name']
            destination_driver_name = connected_config_element['destination_driver_name']
            destination_group_name = connected_config_element['destination_group_name']

            driver_data_provider = DriverDataProvider()
            if source_driver_name != "internal":
                if destination_driver_name in driver_data_provider.get_all_drivers_for_working_type("file"):
                    destination_file_path = connected_config_element['destination_file_path']
                    self.global_config['filemanager'].wait_for_file_creation(destination_file_path)
                    self.global_config['filemanager'].wait_for_file_not_change(destination_file_path)

                    actual_output_messages = self.global_config['filemanager'].get_messages_from_file(destination_file_path)
                elif destination_driver_name in driver_data_provider.get_all_drivers_for_working_type("socket-stream"):
                    if "unix" in destination_driver_name:
                        pass
                        # actual_output_messages = network_based_processor.start_stream_listener(destination_driver_name, destination_group['file_path'])
                    else:
                        pass
                        # actual_output_messages = network_based_processor.start_stream_listener(destination_driver_name)
                elif destination_driver_name in driver_data_provider.get_all_drivers_for_working_type("socket-dgram"):
                    if "unix" in destination_driver_name:
                        pass
                        # actual_output_messages = network_based_processor.start_dgram_listener(destination_driver_name, destination_group['file_path'])
                    else:
                        pass
                        # actual_output_messages = network_based_processor.start_dgram_listener(destination_driver_name)
                else:
                    print("unknown driver_name: %s" % destination_driver_name)
                    assert False
                self.global_config['testrun_reporter'].save_actual_output_messages(destination_group_name, actual_output_messages)

                expected_output_messages = message_generator.generate_output_messages(source_driver_name, destination_driver_name)
                self.global_config['testrun_reporter'].save_expected_output_messages(destination_group_name, expected_output_messages)

    def create_customized_message_parts(self, **kwargs):
        customized_message_parts = {}
        for field_name, field_value in kwargs.items():
            customized_message_parts[field_name] = field_value
        return customized_message_parts
