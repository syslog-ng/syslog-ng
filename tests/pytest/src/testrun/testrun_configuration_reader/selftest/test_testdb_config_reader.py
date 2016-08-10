import os
import pytest
from src.testrun.testrun_configuration_reader.testrun_configuration_reader import TestdbConfigReader
from src.work_with_files.file_based_processor import FileBasedProcessor


@pytest.fixture(scope="function")
def setup_testdb_config_reader(request):
    global fbp, tcr

    testdb_configuration_content = """[Testdb]
RunningTest: func
SyslogNgInstall: path
ConfigVersion: 3.8
InstallDir: /tmp
LogLevel: ERROR
"""
    configuration_file_path = os.path.join("/tmp/", "testdb_configuration.ini")
    fbp = FileBasedProcessor()
    fbp.create_file_with_content(file_path=configuration_file_path, content=testdb_configuration_content)

    tcr = TestdbConfigReader()

    request.addfinalizer(teardown_testdb_config_reader)

@pytest.fixture(scope="function")
def setup_testdb_config_reader_invalid_content(request):
    global fbp, tcr

    testdb_configuration_content = """[Testdb]
aaa: aaa
"""
    configuration_file_path = os.path.join("/tmp/", "testdb_configuration.ini")
    fbp = FileBasedProcessor()
    fbp.create_file_with_content(file_path=configuration_file_path, content=testdb_configuration_content)

    request.addfinalizer(teardown_testdb_config_reader)
    with pytest.raises(ValueError):
        tcr = TestdbConfigReader()


def teardown_testdb_config_reader():
    fbp.delete_written_files()


def test_get_running_test(setup_testdb_config_reader):
    assert "func" == tcr.get_running_test()


def test_get_syslog_ng_install(setup_testdb_config_reader):
    assert "path" == tcr.get_syslog_ng_install()


def test_get_config_version(setup_testdb_config_reader):
    assert "3.8" == tcr.get_config_version()


def test_get_install_dir(setup_testdb_config_reader):
    assert "/tmp" == tcr.get_install_dir()


def test_get_log_level(setup_testdb_config_reader):
    assert "ERROR" == tcr.get_log_level()


def test_validate_config_elements(setup_testdb_config_reader_invalid_content):
    pass