from source.testdb.initializer.setup_classes import SetupClasses


def init_unittest(request):
    sc = SetupClasses(request)
    return sc


def test_connect_driver_with_statement_statement(request):
    sc = init_unittest(request)
    driver_properties = {"test": "test"}
    statement = {'statement': {}}
    assert sc.syslog_ng_config_interface.config_element_connector.connect_driver_with_statement(driver_properties=driver_properties, statement=statement) == {'statement': {'test': 'test'}}


def test_connect_driver_with_statement_statement_name(request):
    sc = init_unittest(request)
    driver_properties = {"test": "test"}
    statement = {'source_statements': {}}
    statement_name = "source_statements"
    assert sc.syslog_ng_config_interface.config_element_connector.connect_driver_with_statement(driver_properties=driver_properties, statement=statement, statement_name=statement_name) == {'source_statements': {'test': 'test'}}


def test_connect_statement_to_root_config(request):
    sc = init_unittest(request)
    statement = {'test': "test"}
    statement_name = "source"
    sc.syslog_ng_config_interface.config_element_connector.connect_statement_to_root_config(statement=statement, statement_name=statement_name)
    assert sc.syslog_ng_config_interface.syslog_ng_config['source_statements'] == statement


def test_connect_statements_in_logpath(request):
    sc = init_unittest(request)
    source_statements = ["src1", "src2"]
    destination_statements = ["dst1,", "dst2"]
    sc.syslog_ng_config_interface.config_element_connector.connect_statements_in_logpath(source_statements=source_statements, destination_statements=destination_statements)
    assert sc.syslog_ng_config_interface.syslog_ng_config['logpaths'] == [{'destination_statements': ['dst1,', 'dst2'], 'source_statements': ['src1', 'src2']}]
