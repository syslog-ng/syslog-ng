import socket
from source.testdb.initializer.setup_classes import SetupClasses


def init_unittest(request):
    sc = SetupClasses(request)
    return sc


def test_save_expected_message(request):
    sc = init_unittest(request)
    src_statement_id, src_driver_id, dst_statement_id, dst_driver_id = sc.syslog_ng_config_interface.create_connected_source_with_destination(source_driver_name="file", destination_driver_name="file")
    sc.syslog_ng_config_interface.generate_config(use_internal_source=False)
    sc.testdb_reporter.save_expected_message()

    # debug
    sc.testdb_reporter.dump_collectors()

    assert sc.testdb_reporter.expected_message_collector == {dst_driver_id: ['Jun  1 08:05:04 %s testprogram[9999]: test ÁÉŐÚŰÓÜ-ááéúóö message 1\n' % socket.gethostname()]}


def test_save_expected_message_with_defined_message_parts(request):
    sc = init_unittest(request)
    src_statement_id, src_driver_id, dst_statement_id, dst_driver_id = sc.syslog_ng_config_interface.create_connected_source_with_destination(source_driver_name="file", destination_driver_name="file")
    sc.syslog_ng_config_interface.generate_config(use_internal_source=False)
    sc.testdb_reporter.save_expected_message(defined_message_parts={"hostname": "test-host"})

    # debug
    sc.testdb_reporter.dump_collectors()

    assert sc.testdb_reporter.expected_message_collector == {dst_driver_id: ['Jun  1 08:05:04 test-host testprogram[9999]: test ÁÉŐÚŰÓÜ-ááéúóö message 1\n']}


def test_save_expected_message_with_defined_message_parts_with_multiple_destinations(request):
    sc = init_unittest(request)
    src_statement_id1, src_driver_id1, dst_statement_id1, dst_driver_id1 = sc.syslog_ng_config_interface.create_connected_source_with_destination(source_driver_name="file", destination_driver_name="file")
    src_statement_id2, src_driver_id2, dst_statement_id2, dst_driver_id2 = sc.syslog_ng_config_interface.create_connected_source_with_destination(source_driver_name="file", destination_driver_name="file")
    sc.syslog_ng_config_interface.generate_config(use_internal_source=False)
    sc.testdb_reporter.save_expected_message(defined_message_parts={"program": "test-program"})

    # debug
    sc.testdb_reporter.dump_collectors()

    assert sc.testdb_reporter.expected_message_collector == {
        dst_driver_id1: ['Jun  1 08:05:04 %s test-program[9999]: test ÁÉŐÚŰÓÜ-ááéúóö message 1\n' % socket.gethostname()],
        dst_driver_id2: ['Jun  1 08:05:04 %s test-program[9999]: test ÁÉŐÚŰÓÜ-ááéúóö message 1\n' % socket.gethostname()]
    }


def test_save_expected_message_counter_3(request):
    sc = init_unittest(request)
    src_statement_id, src_driver_id, dst_statement_id, dst_driver_id = sc.syslog_ng_config_interface.create_connected_source_with_destination(source_driver_name="file", destination_driver_name="file")
    sc.syslog_ng_config_interface.generate_config(use_internal_source=False)
    sc.testdb_reporter.save_expected_message(counter=3)

    # debug
    sc.testdb_reporter.dump_collectors()

    assert sc.testdb_reporter.expected_message_collector == {
        dst_driver_id: [
            'Jun  1 08:05:04 %s testprogram[9999]: test ÁÉŐÚŰÓÜ-ááéúóö message 1\n' % socket.gethostname(),
            'Jun  1 08:05:04 %s testprogram[9999]: test ÁÉŐÚŰÓÜ-ááéúóö message 2\n' % socket.gethostname(),
            'Jun  1 08:05:04 %s testprogram[9999]: test ÁÉŐÚŰÓÜ-ááéúóö message 3\n' % socket.gethostname()
        ]
    }
