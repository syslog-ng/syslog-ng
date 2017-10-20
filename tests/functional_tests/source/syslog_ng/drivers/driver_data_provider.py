import copy


class DriverDataProvider(object):
    def __init__(self, testdb_logger, writers, listeners, registers):
        self.log_writer = testdb_logger.set_logger("DriverDataProvider")
        self.file_register = registers['file']

        self.driver_database = {
            "file": {
                "group_type": "source_destination",
                "driver_properties": {
                    "config_type": "file_based",
                    "connection_type": "file_based",
                    "socket_type": "",
                    "message_format": "rfc3164",
                    "disk_buffer_support": True,
                    "tls_support": False,
                },
                "mandatory_options": {
                    "file_path": "",
                },
                "driver_io": {
                    "writer": writers['file'],
                    "listener": listeners['file'],
                },
            },
            "internal": {
                "group_type": "",
                "driver_properties": {
                    "config_type": "internal",
                    "connection_type": "internal",
                    "socket_type": "",
                    "message_format": "",
                    "disk_buffer_support": False,
                    "tls_support": False,
                },
                "mandatory_options": {},
                "driver_io": {
                    "writer": "",
                    "listener": "",
                },
            },
        }

    def is_driver_in_specified_connection_type(self, driver_name, connection_type):
        return self.driver_database[driver_name]['driver_properties']['connection_type'] is connection_type

    def is_driver_in_specified_config_type(self, driver_name, config_type):
        return self.driver_database[driver_name]['driver_properties']['config_type'] is config_type

    def get_all_source_drivers(self):
        return self.filter_all_drivers_by_group_type(group_type="source")

    def get_all_destination_drivers(self):
        return self.filter_all_drivers_by_group_type(group_type="destination")

    def get_driver_property(self, driver_name, property_name):
        return self.driver_database[driver_name]['driver_properties'][property_name]

    def get_all_drivers_with_property(self, property_name, property_value):
        collected_drivers = []
        for driver in self.driver_database:
            if self.driver_database[driver]['driver_properties'][property_name] == property_value:
                collected_drivers.append(driver)
        return collected_drivers

    def select_mandatory_options_from_defined_options(self, empty_mandatory_options, defined_driver_options):
        for mandatory_option_name in empty_mandatory_options:
            if mandatory_option_name not in defined_driver_options.keys():
                self.log_writer.error("Missing mandatory option [%s] from defined_driver_options: %s" % (mandatory_option_name, defined_driver_options))
                assert False
            for defined_option_name in defined_driver_options:
                if mandatory_option_name == defined_option_name:
                    empty_mandatory_options[mandatory_option_name] = defined_driver_options[defined_option_name]
        selected_mandatory_options = empty_mandatory_options
        return selected_mandatory_options

    def get_config_mandatory_options(self, driver_name, defined_driver_options=None, driver_id=None):
        empty_mandatory_options = copy.deepcopy(self.driver_database[driver_name]['mandatory_options'])
        if defined_driver_options:
            return self.select_mandatory_options_from_defined_options(empty_mandatory_options=empty_mandatory_options, defined_driver_options=defined_driver_options)
        elif driver_id:
            return self.set_mandatory_options_value(empty_mandatory_options=empty_mandatory_options, driver_id=driver_id)

    def get_connection_mandatory_options(self, driver_name, defined_driver_options):
        empty_mandatory_options = copy.deepcopy(self.driver_database[driver_name]['mandatory_options'])
        selected_mandatory_options = self.select_mandatory_options_from_defined_options(empty_mandatory_options=empty_mandatory_options, defined_driver_options=defined_driver_options)
        if self.is_driver_in_specified_config_type(driver_name=driver_name, config_type="file_based"):
            return selected_mandatory_options['file_path']
        elif self.is_driver_in_specified_connection_type(driver_name=driver_name, connection_type="internal"):
            # There is no mandatory option
            pass
        else:
            self.log_writer.error("Unknown driver: %s" % driver_name)
            assert False

    def get_driver_writer(self, driver_name):
        return self.driver_database[driver_name]['driver_io']['writer']

    def get_driver_listener(self, driver_name):
        return self.driver_database[driver_name]['driver_io']['listener']

    def filter_all_drivers_by_group_type(self, group_type):
        collected_drivers = []
        for driver in self.driver_database:
            if group_type in self.driver_database[driver]['group_type']:
                collected_drivers.append(driver)
        return collected_drivers

    def set_mandatory_options_value(self, empty_mandatory_options, driver_id):
        mandatory_options = copy.deepcopy(empty_mandatory_options)
        for option_name in mandatory_options:
            if not mandatory_options[option_name]:
                mandatory_options[option_name] = self.get_option_value_for_option_name(driver_id=driver_id, option_name=option_name)
        return mandatory_options

    def get_option_value_for_option_name(self, driver_id, option_name):
        if option_name == "file_path":
            return self.file_register.get_registered_file_path(prefix=driver_id)
        else:
            self.log_writer.error("Unknown mandatory option: %s" % option_name)
            assert False
