import os

from source.testdb.initializer.setup_classes import SetupClasses


def init_unittest(request):
    sc = SetupClasses(request)
    return sc


def test_syslog_ng_binary_file_paths(request):
    sc = init_unittest(request)
    assert sc.syslog_ng_path_database.get_syslog_ng_binary_path() == os.path.join(sc.testdb_config_context.syslog_ng_install_path, "sbin/syslog-ng")
    assert sc.syslog_ng_path_database.get_syslog_ng_control_tool_path() == os.path.join(sc.testdb_config_context.syslog_ng_install_path, "sbin/syslog-ng-ctl")
    assert sc.syslog_ng_path_database.get_loggen_path() == os.path.join(sc.testdb_config_context.syslog_ng_install_path, "bin/loggen")
    assert sc.syslog_ng_path_database.get_dqtool_path() == os.path.join(sc.testdb_config_context.syslog_ng_install_path, "bin/dqtool")
    assert sc.syslog_ng_path_database.get_pdbtool_path() == os.path.join(sc.testdb_config_context.syslog_ng_install_path, "bin/pdbtool")


def test_syslog_ng_sesssion_file_registrations(request):
    sc = init_unittest(request)

    sc.syslog_ng_path_database.get_control_socket_path()
    sc.syslog_ng_path_database.get_config_path()
    sc.syslog_ng_path_database.get_internal_log_path()
    sc.syslog_ng_path_database.get_stdout_log_path()
    sc.syslog_ng_path_database.get_stderr_log_path()
    sc.syslog_ng_path_database.get_pid_path()
    sc.syslog_ng_path_database.get_persist_state_path()

    topology = "server"
    assert sc.file_register.registered_files['dst_file_internal_%s' % topology].startswith(os.path.join(sc.testdb_path_database.testcase_working_dir, "dst_file_internal_%s" % topology))
    assert sc.file_register.registered_files['stdout_%s' % topology].startswith(os.path.join(sc.testdb_path_database.testcase_working_dir, "stdout_%s" % topology))
    assert sc.file_register.registered_files['stderr_%s' % topology].startswith(os.path.join(sc.testdb_path_database.testcase_working_dir, "stderr_%s" % topology))
    assert sc.file_register.registered_files['pid_%s' % topology].startswith(os.path.join(sc.testdb_path_database.testcase_working_dir, "pid_%s" % topology))
    assert sc.file_register.registered_files['persist_%s' % topology].startswith(os.path.join(sc.testdb_path_database.testcase_working_dir, "persist_%s" % topology))
    assert sc.file_register.registered_files['config_%s' % topology].startswith(os.path.join(sc.testdb_path_database.testcase_working_dir, "config_%s" % topology))
    assert sc.file_register.registered_files['control_socket_%s' % topology].startswith(os.path.join(sc.testdb_path_database.testcase_working_dir, "control_socket_%s" % topology))
