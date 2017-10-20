from source.testdb.initializer.setup_classes import SetupClasses


def init_unittest(request):
    global sc
    sc = SetupClasses(request)


def create_fake_syslog_ng_config(dst_file_path, dst_file_path2):
    statement_id, driver_id = sc.syslog_ng_config_interface.create_source(driver_name="file")
    statement_id2, driver_id2 = sc.syslog_ng_config_interface.create_destination(driver_name="file", driver_options={
        "file_path": dst_file_path}, use_mandatory_options=False)

    statement_id3, driver_id3 = sc.syslog_ng_config_interface.create_source(driver_name="file")
    statement_id4, driver_id4 = sc.syslog_ng_config_interface.create_destination(driver_name="file", driver_options={
        "file_path": dst_file_path2}, use_mandatory_options=False)
    sc.syslog_ng_config_interface.generate_config(use_internal_source=False)
    return driver_id2, driver_id4


def test_threaded_listener(request):
    init_unittest(request)

    dst_file_path = sc.file_register.get_registered_file_path(prefix="unittest", extension="txt")
    dst_file_path2 = sc.file_register.get_registered_file_path(prefix="unittest2", extension="txt")

    dst_driver_id1, dst_driver_id2 = create_fake_syslog_ng_config(dst_file_path, dst_file_path2)

    dst_file_1_content = ["file 1 line 1\n", "file 1 line 2\n"]
    dst_file_2_content = ["file 2 line 1\n", "file 2 line 2\n"]
    sc.file_writer.write_content_to_regular_file(file_path=dst_file_path, content=dst_file_1_content)
    sc.file_writer.write_content_to_regular_file(file_path=dst_file_path2, content=dst_file_2_content)

    sc.threaded_listener.start_listeners(message_counter=2)
    sc.threaded_listener.close_listeners()

    # debug
    sc.testdb_reporter.dump_collectors()

    assert sc.testdb_reporter.actual_message_collector == {
        dst_driver_id1: dst_file_1_content,
        dst_driver_id2: dst_file_2_content
    }
