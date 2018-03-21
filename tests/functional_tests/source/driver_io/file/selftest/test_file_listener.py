from source.testdb.initializer.setup_classes import SetupClasses


def init_unittest(request):
    global sc
    sc = SetupClasses(request)


def create_file_with_content(content):
    global src_file_path
    src_file_path = sc.file_register.get_registered_file_path(prefix="unittest")
    with open(src_file_path, 'w') as file_object:
        file_object.write(content)


def test_get_content_from_regular_file_raw(request):
    init_unittest(request)
    input_message = """test message
test message 2
test message 3
"""
    create_file_with_content(content=input_message)
    assert sc.file_listener.get_content_from_regular_file(file_path=src_file_path, raw=True) == input_message


def test_get_content_from_regular_file_not_raw(request):
    init_unittest(request)
    input_message = """test message
test message 2
test message 3
"""
    create_file_with_content(content=input_message)
    expected_message = ['test message\n', 'test message 2\n', 'test message 3\n']
    assert sc.file_listener.get_content_from_regular_file(file_path=src_file_path, raw=False) == expected_message


def test_wait_for_content(request):
    init_unittest(request)
    input_message = """test message
test message 2
test message 3
"""
    create_file_with_content(content=input_message)
    expected_message = ['test message\n', 'test message 2\n', 'test message 3\n']
    assert sc.file_listener.wait_for_content(file_path=src_file_path, driver_name="file", message_counter=3) == expected_message
