from source.testdb.initializer.setup_classes import SetupClasses


def init_unittest(request):
    sc = SetupClasses(request)
    return sc


def test_testcase_configuration_file_content(request):
    sc = init_unittest(request)
    with open(sc.testdb_path_database.testcase_configuration_file, 'r') as file_object:
        file_content = file_object.read()
        assert "installmode" in file_content
        assert "installpath" in file_content
        assert "syslogngversion" in file_content
        assert "loglevel" in file_content
        assert "runslow" in file_content
