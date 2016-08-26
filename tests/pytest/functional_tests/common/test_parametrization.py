from src.options_and_drivers.option_data_provider import OptionDataProvider

def generate_tests_for_option(metafunc, option_name, option_place):
    option_properties = {
        "option_name": option_name,
        "option_place": option_place,
        "option_value": None,
        "product_version": None
    }
    option_data_provider = OptionDataProvider(option_properties)
    testable_option_values = option_data_provider.get_testable_values()
    testable_product_versions = option_data_provider.get_testable_product_versions()

    parametrize_test_with_argument(metafunc, argument_name="option_name", argument_value=[option_name])
    parametrize_test_with_argument(metafunc, argument_name="option_place", argument_value=[option_place])
    parametrize_test_with_argument(metafunc, argument_name="tested_option_value", argument_value=testable_option_values)
    parametrize_test_with_argument(metafunc, argument_name="tested_product_version", argument_value=testable_product_versions)

def parametrize_test_with_argument(metafunc, argument_name, argument_value):
    metafunc.parametrize(argument_name, argument_value)