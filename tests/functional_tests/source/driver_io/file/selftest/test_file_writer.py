from source.testdb.initializer.setup_classes import SetupClasses


def init_unittest(request):
    sc = SetupClasses(request)
    return sc


def test_write_content_to_regular_file(request):
    sc = init_unittest(request)
    src_file_path = sc.file_register.get_registered_file_path(prefix="unittest")
    assert sc.file_common.is_file_exist(src_file_path) is False
    sc.file_writer.write_content_to_regular_file(file_path=src_file_path, content=["line1"])
    sc.file_writer.write_content_to_regular_file(file_path=src_file_path, content=["line2", "line3\n"])
    sc.file_writer.write_content_to_regular_file(file_path=src_file_path, content="line4\nline5")
    assert sc.file_common.is_file_exist(src_file_path) is True
    with open(src_file_path, 'r') as file_object:
        file_content = file_object.readlines()
        assert ['line1\n', 'line2\n', 'line3\n', 'line4\n', 'line5\n'] == file_content
