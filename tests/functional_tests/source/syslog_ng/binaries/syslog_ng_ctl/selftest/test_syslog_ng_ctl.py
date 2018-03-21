from source.testdb.initializer.setup_classes import SetupClasses


def init_unittest(request, use_valid_config):
    sc = SetupClasses(request)
    if use_valid_config:
        sc.syslog_ng_config_interface.add_global_options(global_options={"stats_level": 3})
        src_statement_id, src_driver_id, dst_statement_id, dst_driver_id = sc.syslog_ng_config_interface.create_connected_source_with_destination(source_driver_name="file", destination_driver_name="file")
        sc.syslog_ng_config_interface.generate_config(use_internal_source=True)
    return sc


def test_syslog_ng_ctl_stats(request):
    sc = init_unittest(request, use_valid_config=True)
    assert sc.syslog_ng.start(expected_run=True) is True
    exit_code, stdout, stderr = sc.syslog_ng_ctl.stats()
    assert exit_code == 0
    assert "global;payload_reallocs;;a;processed;0" in stdout
    assert stderr == ""
    assert sc.syslog_ng.stop() is True


def test_syslog_ng_ctl_stats_reset(request):
    sc = init_unittest(request, use_valid_config=True)
    assert sc.syslog_ng.start(expected_run=True) is True
    assert sc.syslog_ng_ctl.stats(reset=True) == (0, "The statistics of syslog-ng have been reset to 0.\n", "")
    assert sc.syslog_ng.stop() is True


def test_syslog_ng_ctl_stop(request):
    sc = init_unittest(request, use_valid_config=True)
    assert sc.syslog_ng.start(expected_run=True) is True
    assert sc.syslog_ng_ctl.stop() == (0, "OK Shutdown initiated\n", "")
    assert sc.syslog_ng.stop() is True


def test_syslog_ng_ctl_reload(request):
    sc = init_unittest(request, use_valid_config=True)
    assert sc.syslog_ng.start(expected_run=True) is True
    assert sc.syslog_ng_ctl.reload() == (0, "OK Config reload initiated\n", "")
    assert sc.syslog_ng.stop() is True


def test_syslog_ng_ctl_reopen(request):
    sc = init_unittest(request, use_valid_config=True)
    assert sc.syslog_ng.start(expected_run=True) is True
    assert sc.syslog_ng_ctl.reopen() == (0, "OK Re-open of log destination files initiated\n", "")
    assert sc.syslog_ng.stop() is True


def test_syslog_ng_ctl_query_get(request):
    sc = init_unittest(request, use_valid_config=True)
    assert sc.syslog_ng.start(expected_run=True) is True
    exit_code, stdout, stderr = sc.syslog_ng_ctl.query(query_get=True)
    assert exit_code == 0
    assert "global.payload_reallocs.processed=0" in stdout
    assert stderr == ""
    assert sc.syslog_ng.stop() is True


def test_syslog_ng_ctl_query_list(request):
    sc = init_unittest(request, use_valid_config=True)
    assert sc.syslog_ng.start(expected_run=True) is True
    exit_code, stdout, stderr = sc.syslog_ng_ctl.query(query_get=False, query_list=True)
    assert exit_code == 0
    assert "global.payload_reallocs.processed" in stdout
    assert stderr == ""
    assert sc.syslog_ng.stop() is True


def test_syslog_ng_ctl_query_sum(request):
    sc = init_unittest(request, use_valid_config=True)
    assert sc.syslog_ng.start(expected_run=True) is True
    exit_code, stdout, stderr = sc.syslog_ng_ctl.query(query_get=False, query_sum=True)
    assert exit_code == 0
    assert stdout == "0\n"
    assert stderr == ""
    assert sc.syslog_ng.stop() is True


def test_syslog_ng_ctl_query_reset(request):
    sc = init_unittest(request, use_valid_config=True)
    assert sc.syslog_ng.start(expected_run=True) is True
    exit_code, stdout, stderr = sc.syslog_ng_ctl.query(query_get=False, reset=True)
    assert exit_code == 0
    assert "global.payload_reallocs.processed=0\n" in stdout
    assert "The selected counters of syslog-ng have been reset to 0" in stdout
    assert stderr == ""
    assert sc.syslog_ng.stop() is True


def test_syslog_ng_ctl_check_counters(request):
    sc = init_unittest(request, use_valid_config=True)
    sc.threaded_sender.start_senders(counter=1)
    assert sc.syslog_ng.start(expected_run=True) is True
    sc.syslog_ng_ctl.check_counters()
    assert sc.syslog_ng.stop() is True
