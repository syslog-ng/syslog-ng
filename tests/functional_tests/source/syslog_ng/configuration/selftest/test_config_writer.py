from source.testdb.initializer.setup_classes import SetupClasses


def init_unittest(request):
    sc = SetupClasses(request)
    return sc


def test_generate_config_with_internal_source(request):
    sc = init_unittest(request)
    src_statement_id, src_driver_id, dst_statement_id, dst_driver_id = sc.syslog_ng_config_interface.add_internal_source_to_config()
    sc.syslog_ng_config_interface.generate_config(use_internal_source=False)
    syslog_ng_config_path = sc.file_register.get_registered_file_path(prefix="config_server", extension="conf")
    config_content = sc.file_listener.get_content_from_regular_file(file_path=syslog_ng_config_path, raw=True)
    expected_config_content = """@version: %s
@include 'scl.conf'

source %s {
    internal (
    );
};

destination %s {
    file (
        %s
    );
};

log {
    source(%s);
    destination(%s);
};
""" % (
        sc.testdb_config_context.syslog_ng_version,
        src_statement_id,
        dst_statement_id,
        sc.file_register.get_registered_file_path(prefix="destination_internal_log_path"),
        src_statement_id,
        dst_statement_id
    )
    assert config_content == expected_config_content


def test_generate_config_with_custom_source_destination(request):
    sc = init_unittest(request)
    src_statement_id, src_driver_id, dst_statement_id, dst_driver_id = sc.syslog_ng_config_interface.create_connected_source_with_destination(source_driver_name="file", destination_driver_name="file")
    sc.syslog_ng_config_interface.generate_config(use_internal_source=False)
    syslog_ng_config_path = sc.file_register.get_registered_file_path(prefix="config_server", extension="conf")
    config_content = sc.file_listener.get_content_from_regular_file(file_path=syslog_ng_config_path, raw=True)
    expected_config_content = """@version: %s
@include 'scl.conf'

source %s {
    file (
        %s
    );
};

destination %s {
    file (
        %s
    );
};

log {
    source(%s);
    destination(%s);
};
""" % (
        sc.testdb_config_context.syslog_ng_version,
        src_statement_id,
        sc.file_register.get_registered_file_path(prefix=src_driver_id),
        dst_statement_id,
        sc.file_register.get_registered_file_path(prefix=dst_driver_id),
        src_statement_id,
        dst_statement_id
    )
    assert config_content == expected_config_content
