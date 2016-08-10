import pytest
from src.testrun.testrun_validator import testrun_validator
from func_tests.testcase import Testcase
from src.options_and_drivers.option_data_provider import OptionDataProvider

@pytest.fixture(scope="function")
def setup_for_parser_options(request):
    # global install_path, slng_config, slng_config_group_data_provider, slng_process

    option_properties = {
        "option_name": request.getfuncargvalue("option_name"),
        "option_place": request.getfuncargvalue("option_place"),
        "option_value": request.getfuncargvalue("tested_option_value"),
        "product_version": request.getfuncargvalue("tested_product_version")
    }

    option_data_provider = OptionDataProvider(option_properties)

    preconditions = Testcase()
    preconditions.config.add_global_options({"keep_hostname": "yes"})
    s_file = preconditions.config.add_source_group(driver_name=option_data_provider.get_used_drivers_for_driver_type("source")[0])

    parser_options = {option_properties['option_name']: option_properties['option_value']}

    if (option_properties['option_name'] == "database") and (option_properties['option_value'] not in ['not-set', 'empty']):
        parser_options['selector'] = '@@${HOST}@@'
    elif (option_properties['option_name'] == "selector") and (option_properties['option_value'] not in ['not-set', 'empty']):
        parser_options['database'] = "valid-path"
    elif (option_properties['option_name'] == "default_selector") and (option_properties['option_value'] not in ['not-set', 'empty']):
        parser_options['selector'] = '@@${HOST}@@'
        parser_options['database'] = "valid-path"
    elif (option_properties['option_name'] == "prefix") and (option_properties['option_value'] not in ['not-set', 'empty']):
        parser_options['selector'] = '@@${HOST}@@'
        parser_options['database'] = "valid-path"
    else:
        print("Unknown options")


    # database_content = """'mymachine','email','aaa@aaa.com'"""
    p_acd = preconditions.config.add_parser_group(driver_name="add_contextual_data", options=parser_options)
    d_file = preconditions.config.add_destination_group(driver_name=option_data_provider.get_used_drivers_for_driver_type("destination")[0])
    preconditions.config.connect_drivers_in_logpath(sources=[s_file], parsers=[p_acd], destinations=[d_file])

    preconditions.config.add_internal_source_to_config()
    preconditions.config.render_config(preconditions.syslog_ng.get_syslog_ng_config_path())
    preconditions.syslog_ng.start()

    request.addfinalizer(teardown_parser_acd)

    # install_path = testrun_configuration_provider.get_install_path_for_product_version(option_properties['product_version'])
    # testrun_validator.validate_testrun(option_properties['product_version'], install_path)
    #
    # slng_config_generator = SlngConfigGenerator(option_properties, install_path)
    # slng_config = slng_config_generator.generate_config_for_option()
    #
    # slng_config_group_data_provider = SlngConfigGroupDataProvider(slng_config)
    #
    # slng_process = SyslogNg(install_path, slng_config_group_data_provider)
    # slng_process.start()
    #
    # if slng_process:
    #     request.addfinalizer(teardown_test_for_option)



@pytest.fixture(scope="function")
def setup_parser_acd(request):
    global preconditions
    print("\n-->SETUP")
    # testrun_validator.is_concurrent_syslog_daemon_running()

    preconditions = request.getfuncargvalue("preconditions")

    preconditions.config.add_internal_source_to_config()
    preconditions.config.render_config(preconditions.syslog_ng.get_syslog_ng_config_path())
    preconditions.syslog_ng.start()

    request.addfinalizer(teardown_parser_acd)


def teardown_parser_acd():
    print("\n-->TEARDOWN")
    preconditions.syslog_ng.stop()
