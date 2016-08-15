import logging
from src.options_and_drivers import db_parser_options
from src.options_and_drivers.driver_data_provider import DriverDataProvider


class OptionDataProvider(object):
    def __init__(self, option_properties):
        self.option_name = option_properties['option_name']
        self.option_place = option_properties['option_place']
        self.option_value = option_properties['option_value']
        self.product_version = option_properties['product_version']
        self.option_database = self.get_option_database_for_option_place()
        self.validate_option_properties()
        self.driver_data_provider = DriverDataProvider()

    def get_all_properties(self):
        return self.option_database[self.option_name]

    def get_testable_values(self):
        return self.option_database[self.option_name]['option_values']

    def get_testable_product_versions(self):
        return self.option_database[self.option_name]['product_versions']

    def get_additional_further_options(self, further_option_type):
        return self.option_database[self.option_name]['further_%s_options' % further_option_type]

    def get_used_drivers_for_driver_type(self, target_driver_type):
        if target_driver_type == "source":
            opposite_driver_type = "destination"
            used_drivers_list_for_target_driver_type = self.option_database[self.option_name]['source_drivers']
        else:
            opposite_driver_type = "source"
            used_drivers_list_for_target_driver_type = self.option_database[self.option_name]['destination_drivers']

        if "all_available" in used_drivers_list_for_target_driver_type:
            return self.driver_data_provider.get_all_drivers_for_a_product_version(self.product_version, target_driver_type)
        elif "fill_up_with" in used_drivers_list_for_target_driver_type[0]:
            used_drivers_list_for_opposite_driver_type = self.get_used_drivers_for_driver_type(opposite_driver_type)
            return get_list_of_filled_up_drivers(used_drivers_list_for_target_driver_type, used_drivers_list_for_opposite_driver_type)
        else:
            return get_filtered_driver_list_for_product_version(used_drivers_list_for_target_driver_type, self.product_version, target_driver_type)

    def get_option_database_for_option_place(self):
        if self.option_place == "source_options":
            pass
        elif self.option_place == "destination_options":
            pass
        elif self.option_place == "global_options":
            pass
        elif self.option_place == "parser_options":
            return db_parser_options.parser_options
        else:
            logging.error("Unknown option_place: %s" % self.option_place)
            raise SystemExit(1)

    def is_option_value_default(self):
        return self.option_value == "default"

    def is_option_value_empty(self):
        return self.option_value == "empty"

    def is_actual_option_in_target_statement_option(self, target_statement):
        return self.option_place == "%s_options" % target_statement

    def format_option_properties_for_config(self):
        return {"option_name": self.option_name, "option_value": self.option_value}

    def validate_option_properties(self):
        self.validate_option_keys()
        self.validate_option_name()

    def validate_option_keys(self):
        for key in self.option_database[self.option_name].keys():
            if key not in ['option_values', 'destination_drivers', 'parser_drivers', 'source_drivers', 'product_versions', 'further_global_options', 'further_destination_options', 'further_source_options']:
                logging.error("option key value: [%s] unknown value.")
                raise SystemExit(1)

    def validate_option_name(self):
        if self.option_name not in self.option_database:
            logging.error("option name: %s not in %s database" % (self.option_name, self.option_place))
            raise SystemExit(1)

def get_list_of_filled_up_drivers(used_drivers_in_target_driver_type, used_drivers_in_opposite_driver_type):
    filled_driver_list = []
    filled_driver_name = used_drivers_in_target_driver_type[0].split("_")[-1]
    for driver_name in used_drivers_in_opposite_driver_type:
        filled_driver_list.append(filled_driver_name)
    return filled_driver_list

def get_filtered_driver_list_for_product_version(used_drivers_in_target_driver_type, product_version, driver_type):
    filtered_driver_list_for_product_version = []
    explicit_driver_list = used_drivers_in_target_driver_type
    for driver_name in explicit_driver_list:
        if driver_name in DriverDataProvider().get_all_drivers_for_a_product_version(product_version, driver_type):
            filtered_driver_list_for_product_version.append(driver_name)
    return filtered_driver_list_for_product_version
