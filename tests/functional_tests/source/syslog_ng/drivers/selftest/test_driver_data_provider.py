from source.testdb.initializer.setup_classes import SetupClasses


def init_unittest(request):
    sc = SetupClasses(request)
    return sc


def test_get_all_source_drivers(request):
    sc = init_unittest(request)
    assert sorted(sc.driver_data_provider.get_all_source_drivers()) == sorted(['file'])


def test_get_all_destination_drivers(request):
    sc = init_unittest(request)
    assert sc.driver_data_provider.get_all_destination_drivers() == ['file']


def test_is_driver_in_specified_connection_type(request):
    sc = init_unittest(request)
    assert sc.driver_data_provider.is_driver_in_specified_connection_type(
        driver_name="file", connection_type="file_based") is True


def test_is_driver_in_specified_config_type(request):
    sc = init_unittest(request)
    assert sc.driver_data_provider.is_driver_in_specified_config_type(
        driver_name="file", config_type="file_based") is True


def test_get_driver_writer(request):
    sc = init_unittest(request)
    assert issubclass(sc.driver_data_provider.get_driver_writer(
        driver_name="file").__class__, sc.file_writer.__class__) is True


def test_get_driver_listener(request):
    sc = init_unittest(request)
    assert issubclass(sc.driver_data_provider.get_driver_listener(
        driver_name="file").__class__, sc.file_listener.__class__) is True


def test_get_driver_property(request):
    sc = init_unittest(request)
    assert sc.driver_data_provider.get_driver_property(driver_name="file", property_name="message_format") == "rfc3164"


def test_get_config_mandatory_options_with_predefined_driver_options(request):
    sc = init_unittest(request)
    defined_driver_options = {"some_option": "some_option_value",
                              "file_path": "mypath", "some_option2": "some_option_value2", "ip": "181.12.2.1"}
    assert sc.driver_data_provider.get_config_mandatory_options(
        driver_name="file", defined_driver_options=defined_driver_options) == {"file_path": "mypath"}


def test_get_config_mandatory_options(request):
    sc = init_unittest(request)
    driver_id = "asdf1234"
    mandatory_option_for_file = sc.driver_data_provider.get_config_mandatory_options(
        driver_name="file", driver_id=driver_id)
    assert "file_path" in mandatory_option_for_file.keys()
    assert driver_id in mandatory_option_for_file['file_path']


def test_get_connection_mandatory_options_with_predefined_driver_options(request):
    sc = init_unittest(request)
    defined_driver_options = {"some_option": "some_option_value",
                              "file_path": "mypath", "some_option2": "some_option_value2", "ip": "181.12.2.1"}
    assert sc.driver_data_provider.get_connection_mandatory_options(
        driver_name="file", defined_driver_options=defined_driver_options) == "mypath"
