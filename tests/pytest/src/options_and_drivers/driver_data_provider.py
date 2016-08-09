from src.options_and_drivers import db_drivers

class DriverDataProvider(object):
    def __init__(self):
        self.driver_database = db_drivers.driver_database

    def get_all_drivers(self):
        return self.driver_database.keys()

    def get_resource_for_a_driver(self, driver_name):
        return self.__get_specified_information_for_a_driver(driver_name, "resource")

    def get_required_source_options_for_a_driver(self, driver_name):
        return self.__get_specified_information_for_a_driver(driver_name, "required_source_options")

    def get_required_destination_options_for_a_driver(self, driver_name):
        return self.__get_specified_information_for_a_driver(driver_name, "required_destination_options")

    def get_required_option_for_a_driver(self, driver_name, driver_type):
        return self.__get_specified_information_for_a_driver(driver_name, "required_%s_options" % driver_type)

    def get_working_type_for_a_driver(self, driver_name):
        return self.__get_specified_information_for_a_driver(driver_name, "working_type")

    def __get_specified_information_for_a_driver(self, driver_name, specified_information):
        return self.driver_database[driver_name][specified_information]

    def get_all_source_drivers_for_a_product_version(self, product_version):
        return self.get_all_drivers_for_a_product_version(product_version, "source")

    def get_all_destination_drivers_for_a_product_version(self, product_version):
        return self.get_all_drivers_for_a_product_version(product_version, "destination")

    def get_all_drivers_for_a_product_version(self, product_version, driver_type):
        drivers_for_product_version = []
        for driver_name in self.get_all_drivers():
            if product_version in self.__get_specified_information_for_a_driver(driver_name, 'used_as_%s_driver_in_following_product_versions' % driver_type):
                drivers_for_product_version.append(driver_name)
        return drivers_for_product_version

    def get_all_drivers_for_working_type(self, working_type):
        drivers_for_working_type = []
        for driver_name in self.get_all_drivers():
            if working_type in self.__get_specified_information_for_a_driver(driver_name, "working_type"):
                drivers_for_working_type.append(driver_name)
        return drivers_for_working_type

    def get_all_source_drivers_with_a_required_option(self, required_option):
        return self.get_all_drivers_with_a_required_option(required_option, "source")

    def get_all_destination_drivers_with_a_required_option(self, required_option):
        return self.get_all_drivers_with_a_required_option(required_option, "destination")

    def get_all_drivers_with_a_required_option(self, required_option, driver_type):
        drivers_with_required_option = []
        for driver_name in self.get_all_drivers():
            required_options_for_a_driver = self.__get_specified_information_for_a_driver(driver_name, "required_%s_options" % driver_type)
            if required_options_for_a_driver:
                for required_option_for_a_driver in required_options_for_a_driver:
                    if required_option in required_option_for_a_driver['option_name']:
                        drivers_with_required_option.append(driver_name)
        return drivers_with_required_option
