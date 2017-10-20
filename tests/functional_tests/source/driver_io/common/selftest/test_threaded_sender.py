from source.testdb.initializer.setup_classes import SetupClasses


def init_unittest(request):
    global sc
    sc = SetupClasses(request)


def create_fake_syslog_ng_config():
    src_statement_id, src_driver_id, dst_statement_id, dst_driver_id = sc.syslog_ng_config_interface.create_connected_source_with_destination(
        source_driver_name="file", destination_driver_name="file")
    src_statement_id2, src_driver_id2, dst_statement_id2, dst_driver_id2 = sc.syslog_ng_config_interface.create_connected_source_with_destination(
        source_driver_name="file", destination_driver_name="file")

    sc.syslog_ng_config_interface.generate_config(use_internal_source=False)
    return src_driver_id, src_driver_id2


def test_threaded_sender(request):
    init_unittest(request)

    src_driver_id, src_driver_id2 = create_fake_syslog_ng_config()

    defined_message_parts = {"hostname": "AAAA"}

    sc.threaded_sender.start_senders(defined_message_parts, counter=2)

    # debug
    sc.testdb_reporter.dump_collectors()

    assert sc.testdb_reporter.input_message_collector == {
        src_driver_id: [
            '<38>Jun  1 08:05:04 AAAA testprogram[9999]: test ÁÉŐÚŰÓÜ-ááéúóö message 1',
            '<38>Jun  1 08:05:04 AAAA testprogram[9999]: test ÁÉŐÚŰÓÜ-ááéúóö message 2'
        ],
        src_driver_id2: [
            '<38>Jun  1 08:05:04 AAAA testprogram[9999]: test ÁÉŐÚŰÓÜ-ááéúóö message 1',
            '<38>Jun  1 08:05:04 AAAA testprogram[9999]: test ÁÉŐÚŰÓÜ-ááéúóö message 2'
        ],
    }
