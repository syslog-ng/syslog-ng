import signal
import pytest

from source.testdb.initializer.setup_classes import SetupClasses

slow = pytest.mark.skipif(
    not pytest.config.getoption("--runslow"),
    reason="need --runslow option to run"
)


def init_unittest(request, use_valid_config, topology="server"):
    global sc
    sc = SetupClasses(testcase_context=request, topology=topology)
    if use_valid_config:
        if topology == "client_server":
            sc.syslog_ng_config_interface_for_client.generate_config(use_internal_source=True)
            sc.syslog_ng_config_interface_for_server.generate_config(use_internal_source=True)
        else:
            sc.syslog_ng_config_interface.generate_config(use_internal_source=True)
    else:
        sc.syslog_ng_config_interface.generate_raw_config(raw_config=get_invalid_config())
    return sc


def get_invalid_config():
    invalid_config = """@version: %s
source s_file{ afile("/tmp/aaa"); };
destination d_file{ file("/tmp/bbb"); };
log{source(s_file); destination(d_file);};""" % sc.testdb_config_context.syslog_ng_version
    return invalid_config


def get_valid_config():
    valid_config = """@version: %s
source s_file{ file("/tmp/aaa"); };
destination d_file{ file("/tmp/bbb"); };
log{source(s_file); destination(d_file);};""" % sc.testdb_config_context.syslog_ng_version
    return valid_config


def test_syslog_ng_start_normal(request):
    sc = init_unittest(request, use_valid_config=True)
    assert sc.syslog_ng.start(expected_run=True) is True
    assert sc.syslog_ng.stop() is True


def test_syslog_ng_start_failed(request):
    sc = init_unittest(request, use_valid_config=False)
    with pytest.raises(AssertionError):
        sc.syslog_ng.start(expected_run=True)


def test_syslog_ng_start_expected_failed(request):
    sc = init_unittest(request, use_valid_config=False)
    assert sc.syslog_ng.start(expected_run=False) is True


def test_syslog_ng_reload_normal(request):
    sc = init_unittest(request, use_valid_config=True)
    assert sc.syslog_ng.start(expected_run=True) is True
    assert sc.syslog_ng.reload() is True
    assert sc.syslog_ng.stop() is True


def test_syslog_ng_reload_failed(request):
    # Before we send reload syslog-ng is already stopped
    sc = init_unittest(request, use_valid_config=True)
    assert sc.syslog_ng.start(expected_run=True) is True
    sc.executor_interface.send_signal_to_process(process=sc.syslog_ng.get_syslog_ng_process(), process_signal=signal.SIGTERM)
    with pytest.raises(AssertionError):
        sc.syslog_ng.reload()


def test_syslog_ng_stop_failed(request):
    # Before we send stop syslog-ng is already stopped
    sc = init_unittest(request, use_valid_config=True)
    assert sc.syslog_ng.start(expected_run=True) is True
    sc.executor_interface.send_signal_to_process(process=sc.syslog_ng.get_syslog_ng_process(), process_signal=signal.SIGTERM)
    with pytest.raises(AssertionError):
        sc.syslog_ng.stop()


def test_check_generated_core_file(request):
    sc = init_unittest(request, use_valid_config=True)
    assert sc.syslog_ng.start(expected_run=True) is True
    sc.executor_interface.send_signal_to_process(process=sc.syslog_ng.get_syslog_ng_process(), process_signal=signal.SIGSEGV)
    with pytest.raises(AssertionError):
        sc.syslog_ng.is_core_file_exist()


@slow
def test_syslog_ng_with_external_tool(request):
    sc = init_unittest(request, use_valid_config=True)
    assert sc.syslog_ng.start(expected_run=True, external_tool="valgrind") is True
    assert sc.executor_interface.get_pid_for_process_name(substring="valgrind") is not None
    sc.syslog_ng.stop()
    assert sc.executor_interface.get_pid_for_process_name(substring="valgrind") is None

@slow
def test_syslog_ng_client_server_start_stop(request):
    sc = init_unittest(request=request, use_valid_config=True, topology="client_server")
    sc.syslog_ng_for_client.start()
    sc.syslog_ng_for_server.start()

    sc.syslog_ng_for_client.stop()
    sc.syslog_ng_for_server.stop()
