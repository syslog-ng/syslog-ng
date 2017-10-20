from source.testdb.initializer.setup_testcase import SetupTestcase

def test_use_raw_config(request):
    syslog_ng_tc = SetupTestcase(testcase_context=request)

    src_file_path = syslog_ng_tc.file_register.get_registered_file_path(prefix="src_file")
    dst_file_path = syslog_ng_tc.file_register.get_registered_file_path(prefix="dst_file")
    raw_config = """@version: %s
source s_file{file("%s");};
destination d_file{file("%s");};
log{source(s_file); destination(d_file);};
""" % (syslog_ng_tc.testdb_config_context.syslog_ng_version, src_file_path, dst_file_path)
    syslog_ng_tc.syslog_ng_config_interface.generate_raw_config(raw_config=raw_config)

    input_message = syslog_ng_tc.message_interface.create_message_for_source_driver(driver_name="file")
    syslog_ng_tc.file_writer.write_content_to_regular_file(file_path=src_file_path, content=input_message)

    syslog_ng_tc.syslog_ng.start(external_tool="strace")
    actual_message = syslog_ng_tc.file_listener.get_content_from_regular_file(file_path=dst_file_path)

    expected_output_message = syslog_ng_tc.message_interface.create_message_for_destination_driver(driver_name="file")
    assert actual_message == expected_output_message[0]
    syslog_ng_tc.syslog_ng_ctl.check_counters()
    syslog_ng_tc.executor_interface.dump_process_information(process=syslog_ng_tc.syslog_ng.syslog_ng_process)
