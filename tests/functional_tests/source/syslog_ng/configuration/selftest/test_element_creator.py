import pytest
from source.testdb.initializer.setup_classes import SetupClasses


def init_unittest(request):
    global driver_property_keys
    sc = SetupClasses(request)
    driver_property_keys = sorted(
        ['config_mandatory_options', 'writer_class', 'driver_name', 'connection_mandatory_options', 'listener_class',
         'driver_options'])
    return sc


def test_create_driver_properties(request):
    sc = init_unittest(request)
    driver_properties, driver_id = sc.syslog_ng_config_interface.create_driver_properties(statement_id="asdf1234",
                                                                                          driver_name="file",
                                                                                          driver_options="",
                                                                                          use_mandatory_options=True)
    file_source_path = sc.file_register.get_registered_file_path(prefix=driver_id)
    assert driver_properties == {driver_id: {
        'config_mandatory_options': {"file_path": file_source_path},
        'writer_class': sc.driver_data_provider.get_driver_writer(driver_name="file"),
        'driver_name': "file",
        'connection_mandatory_options': file_source_path,
        'listener_class': sc.driver_data_provider.get_driver_listener(driver_name="file"),
        'driver_options': {"file_path": file_source_path}
    }}


def test_create_driver_properties_with_driver_options(request):
    sc = init_unittest(request)
    driver_properties, driver_id = sc.syslog_ng_config_interface.create_driver_properties(statement_id="asdf1234",
                                                                                          driver_name="file",
                                                                                          driver_options={
                                                                                              "test_option": "test_value"},
                                                                                          use_mandatory_options=True)
    file_source_path = sc.file_register.get_registered_file_path(prefix=driver_id)
    assert driver_properties == {driver_id: {
        'config_mandatory_options': {"file_path": file_source_path},
        'writer_class': sc.driver_data_provider.get_driver_writer(driver_name="file"),
        'driver_name': "file",
        'connection_mandatory_options': file_source_path,
        'listener_class': sc.driver_data_provider.get_driver_listener(driver_name="file"),
        'driver_options': {
            "file_path": file_source_path,
            "test_option": "test_value",
        }
    }}


def test_create_driver_properties_with_driver_options_use_mandatory_option_false(request):
    sc = init_unittest(request)
    with pytest.raises(AssertionError):
        driver_properties, driver_id = sc.syslog_ng_config_interface.create_driver_properties(statement_id="asdf1234",
                                                                                              driver_name="file",
                                                                                              driver_options={
                                                                                                  "test_option": "test_value"},
                                                                                              use_mandatory_options=False)


def test_create_source_group_with_one_driver(request):
    sc = init_unittest(request)
    statement_id, driver_id = sc.syslog_ng_config_interface.create_source(driver_name="file")
    assert sorted(sc.syslog_ng_config_interface.syslog_ng_config['source_statements'][statement_id][driver_id].keys()) == driver_property_keys


def test_create_source_group_with_defined_statement_used_mandatory_options_true(request):
    sc = init_unittest(request)
    driver_name = "file"
    defined_driver_options = {
        "file_path": "mypath",
        "test_option": "test_value"
    }
    statement_id, driver_id = sc.syslog_ng_config_interface.create_source(driver_name=driver_name,
                                                                          driver_options=defined_driver_options,
                                                                          use_mandatory_options=True)

    expected_driver_properties = {
        statement_id: {
            driver_id: {
                'config_mandatory_options': {"file_path": sc.file_register.get_registered_file_path(
                    prefix="%s_%s" % (statement_id, driver_name))},
                'writer_class': sc.driver_data_provider.get_driver_writer(driver_name="file"),
                'driver_name': driver_name,
                'connection_mandatory_options': sc.file_register.get_registered_file_path(
                    prefix="%s_%s" % (statement_id, driver_name)),
                'listener_class': sc.driver_data_provider.get_driver_listener(driver_name="file"),
                'driver_options': {
                    "file_path": sc.file_register.get_registered_file_path(
                        prefix="%s_%s" % (statement_id, driver_name)),
                    "test_option": "test_value",
                }
            }
        }
    }
    assert sc.syslog_ng_config_interface.syslog_ng_config['source_statements'] == expected_driver_properties


def test_create_source_group_with_defined_statement_used_mandatory_options_false(request):
    sc = init_unittest(request)
    driver_name = "file"
    defined_driver_options = {
        "file_path": "mypath",
        "test_option": "test_value"
    }
    statement_id, driver_id = sc.syslog_ng_config_interface.create_source(driver_name=driver_name,
                                                                          driver_options=defined_driver_options,
                                                                          use_mandatory_options=False)

    expected_driver_properties = {
        statement_id: {
            driver_id: {
                'config_mandatory_options': {"file_path": "mypath"},
                'writer_class': sc.driver_data_provider.get_driver_writer(driver_name="file"),
                'driver_name': driver_name,
                'connection_mandatory_options': "mypath",
                'listener_class': sc.driver_data_provider.get_driver_listener(driver_name="file"),
                'driver_options': {
                    "file_path": "mypath",
                    "test_option": "test_value",
                }
            }
        }
    }
    assert sc.syslog_ng_config_interface.syslog_ng_config['source_statements'] == expected_driver_properties


def test_create_destination_group_with_two_drivers(request):
    sc = init_unittest(request)
    statement_id1, driver_id1 = sc.syslog_ng_config_interface.create_destination(driver_name="file")
    statement_id2, driver_id2 = sc.syslog_ng_config_interface.create_destination(driver_name="file")
    assert sorted(sc.syslog_ng_config_interface.syslog_ng_config['destination_statements'][statement_id1][driver_id1].keys()) == driver_property_keys
    assert sorted(sc.syslog_ng_config_interface.syslog_ng_config['destination_statements'][statement_id2][driver_id2].keys()) == driver_property_keys


def test_create_connected_source_with_destination(request):
    sc = init_unittest(request)
    src_statement_id, src_driver_id, dst_statement_id, dst_driver_id = sc.syslog_ng_config_interface.create_connected_source_with_destination(source_driver_name="file", destination_driver_name="file")
    assert sorted(sc.syslog_ng_config_interface.syslog_ng_config['source_statements'][src_statement_id][src_driver_id].keys()) == driver_property_keys
    assert sorted(sc.syslog_ng_config_interface.syslog_ng_config['destination_statements'][dst_statement_id][dst_driver_id].keys()) == driver_property_keys
    assert sc.syslog_ng_config_interface.syslog_ng_config['logpaths'][0]['source_statements'] == [src_statement_id]
    assert sc.syslog_ng_config_interface.syslog_ng_config['logpaths'][0]['destination_statements'] == [dst_statement_id]


def test_add_internal_source_to_config(request):
    sc = init_unittest(request)
    src_statement_id, src_driver_id, dst_statement_id, dst_driver_id = sc.syslog_ng_config_interface.add_internal_source_to_config()
    expected_syslog_ng_config = {
        "version": sc.testdb_config_context.syslog_ng_version,
        "include": ["scl.conf"],
        "module": [],
        "define": {},
        "channel": [],
        "block": [],
        "global_options": {},
        "source_statements": {
            src_statement_id: {
                src_driver_id: {
                    'config_mandatory_options': {},
                    'connection_mandatory_options': None,
                    'driver_name': "internal",
                    'driver_options': {},
                    'listener_class': '',
                    'writer_class': '',
                }
            }
        },
        "parser_statements": [],
        "template_statements": [],
        "rewrite_statements": [],
        "filter_statements": [],
        "destination_statements": {
            dst_statement_id: {
                dst_driver_id: {
                    'config_mandatory_options': {
                        "file_path": sc.file_register.get_registered_file_path(prefix="destination_internal_log_path")},
                    'connection_mandatory_options': sc.file_register.get_registered_file_path(
                        prefix="destination_internal_log_path"),
                    'driver_name': "file",
                    'driver_options': {
                        "file_path": sc.file_register.get_registered_file_path(prefix="destination_internal_log_path")},
                    'listener_class': sc.driver_data_provider.get_driver_listener(driver_name="file"),
                    'writer_class': sc.driver_data_provider.get_driver_writer(driver_name="file"),
                }
            }
        },
        "logpaths": [{
            'destination_statements': [dst_statement_id],
            'source_statements': [src_statement_id]
        }],
    }
    assert sc.syslog_ng_config_interface.syslog_ng_config == expected_syslog_ng_config
