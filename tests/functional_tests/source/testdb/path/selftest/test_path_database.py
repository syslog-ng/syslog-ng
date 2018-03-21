import os
from source.testdb.initializer.setup_classes import SetupClasses


def init_unittest(request):
    sc = SetupClasses(request)
    return sc


def test_testdb_root_dir(request):
    sc = init_unittest(request)
    assert os.path.exists(sc.testdb_path_database.testdb_root_dir) is True


def test_testdb_report_dir(request):
    sc = init_unittest(request)
    assert sc.testdb_path_database.testdb_report_dir == os.path.join(sc.testdb_path_database.testdb_root_dir, 'reports')
    assert os.path.exists(sc.testdb_path_database.testdb_root_dir) is True


def test_testcase_configuration_file(request):
    sc = init_unittest(request)
    assert sc.testdb_path_database.testcase_configuration_file.endswith('testcase_configuration.ini')
    assert os.path.exists(sc.testdb_path_database.testcase_configuration_file) is True


def test_testcase_report_file(request):
    sc = init_unittest(request)
    assert sc.testdb_path_database.testcase_report_file.endswith('testcase_test_testcase_report_file_server.log')
    assert os.path.exists(sc.testdb_path_database.testcase_report_file) is True
    with open(sc.testdb_path_database.testcase_report_file, 'r') as file_object:
        testcase_reportfile_content = file_object.read()
        assert "First line" in testcase_reportfile_content
