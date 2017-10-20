from source.testdb.initializer.setup_testcase import SetupTestcase

def test_use_single_drivers_default(request):
    syslog_ng_tc = SetupTestcase(testcase_context=request)
    syslog_ng_tc.build_config_with_single_drivers(source_driver="file", destination_driver="file")
    syslog_ng_tc.run()

def test_use_single_drivers_with_templated_destination(request):
    syslog_ng_tc = SetupTestcase(testcase_context=request)
    global_options = {"stats_level": 3, "threaded": "yes"}
    source_options = {"follow_freq": 10}
    destination_options = {"template": "'$MESSAGE\n'"}
    syslog_ng_tc.build_config_with_single_drivers(source_driver="file", destination_driver="file", global_options=global_options, source_options=source_options, destination_options=destination_options)
    expected_raw_message = "test ÁÉŐÚŰÓÜ-ááéúóö message 1\n"
    syslog_ng_tc.run(expected_raw_message=expected_raw_message)

def test_use_single_drivers_with_format_json_destination(request):
    syslog_ng_tc = SetupTestcase(testcase_context=request)
    global_options = {"stats_level": 3, "threaded": "yes"}
    source_options = {"follow_freq": 10}
    destination_options = {"template": "'$(format-json --scope rfc3164)\n'"}
    syslog_ng_tc.build_config_with_single_drivers(source_driver="file", destination_driver="file", global_options=global_options, source_options=source_options, destination_options=destination_options)
    expected_raw_message = '{"PROGRAM":"testprogram","PRIORITY":"info","PID":"9999","MESSAGE":"test ÁÉŐÚŰÓÜ-ááéúóö message 1","HOST":"tristram","FACILITY":"auth","DATE":"Jun  1 08:05:04"}\n'
    syslog_ng_tc.run(expected_raw_message=expected_raw_message)

def test_use_single_drivers_with_custom_input_message(request):
    syslog_ng_tc = SetupTestcase(testcase_context=request)
    global_options = {"stats_level": 1, "keep_hostname": "yes"}
    syslog_ng_tc.build_config_with_single_drivers(source_driver="file", destination_driver="file", global_options=global_options)
    defined_message_parts = {
        "priority": "99",
        "bsd_timestamp": "Dec  1 08:05:04",
        "hostname": "test-machine",
        "program": "test-program",
        "pid": "1111",
        "message": "Test message"
    }
    message_counter=100
    syslog_ng_tc.threaded_listener.start_listeners(message_counter=message_counter)

    for i in range(1, 6):
        syslog_ng_tc.threaded_sender.start_senders(defined_message_parts=defined_message_parts, counter=message_counter)
        syslog_ng_tc.syslog_ng.start()
        syslog_ng_tc.syslog_ng.reload()
        syslog_ng_tc.testdb_reporter.save_expected_message(defined_message_parts=defined_message_parts, counter=message_counter)
        syslog_ng_tc.syslog_ng_ctl.check_counters(message_counter=message_counter)
        syslog_ng_tc.syslog_ng.stop()

    syslog_ng_tc.threaded_listener.close_listeners()
    syslog_ng_tc.testdb_reporter.compare_collected_messages()
