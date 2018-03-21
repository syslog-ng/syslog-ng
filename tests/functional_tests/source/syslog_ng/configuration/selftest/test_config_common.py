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
    src_statement_id, src_driver_id, dst_statement_id, dst_driver_id = sc.syslog_ng_config_interface.create_connected_source_with_destination(source_driver_name="file", destination_driver_name="file")
    sc.syslog_ng_config_interface.generate_config()
    assert sc.syslog_ng_config_interface.get_logpath_node_where_statment_exist(statement_id=dst_statement_id) == {'source_statements': [src_statement_id], 'destination_statements': [dst_statement_id]}
    assert sc.syslog_ng_config_interface.get_connected_source_statements_where_statement_exist(statement_id=dst_statement_id) == [src_statement_id]
    assert sc.syslog_ng_config_interface.get_connected_destination_statements_where_statement_exist(statement_id=src_statement_id) == [dst_statement_id]
