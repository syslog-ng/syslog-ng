import filecmp
import os
from source.testdb.initializer.setup_classes import SetupClasses


def init_unittest(request):
    global sc
    sc = SetupClasses(request)


def create_file_with_content(content, src_file_path=None):
    if not src_file_path:
        src_file_path = sc.file_register.get_registered_file_path(prefix="unittest")
    with open(src_file_path, 'w') as file_object:
        file_object.write(content)
    return src_file_path


def create_empty_dir():
    dir_path = sc.file_register.get_registered_dir_path(prefix="unittest")
    sc.file_common.create_dir(dir_path)
    return dir_path


def test_is_file_exist_true(request):
    init_unittest(request)
    file_path = sc.file_register.get_registered_file_path(prefix="unittest")
    with open(file_path, 'w') as file_object:
        file_object.write("test message")
    assert sc.file_common.is_file_exist(file_path=file_path) is True


def test_is_file_exist_false(request):
    init_unittest(request)
    assert sc.file_common.is_file_exist(file_path="randomfile") is False


def test_wait_for_file_creation_true(request):
    init_unittest(request)
    file_path = sc.file_register.get_registered_file_path(prefix="unittest")
    with open(file_path, 'w') as file_object:
        file_object.write("test message")
    assert sc.file_common.wait_for_file_creation(file_path=file_path) is True


def test_wait_for_file_creation_false(request):
    init_unittest(request)
    file_path = sc.file_register.get_registered_file_path(prefix="unittest")
    assert sc.file_common.wait_for_file_creation(file_path=file_path, monitoring_time=0.2) is False


def test_copy_file(request):
    init_unittest(request)
    src_file_path = create_file_with_content(content="First line\n")
    dir_path = create_empty_dir()
    dst_file_path = sc.file_register.get_registered_file_path(prefix="unittest2", subdir=dir_path.split("/")[-1])
    sc.file_common.copy_file(source_path=src_file_path, destination_path=dst_file_path)
    assert filecmp.cmp(src_file_path, dst_file_path) is True


def test_move_file(request):
    init_unittest(request)
    content = "First line\n"
    src_file_path = create_file_with_content(content=content)
    dir_path = create_empty_dir()
    dst_file_path = sc.file_register.get_registered_file_path(prefix="unittest2", subdir=dir_path.split("/")[-1])
    sc.file_common.move_file(source_path=src_file_path, destination_path=dst_file_path)
    assert sc.file_common.is_file_exist(src_file_path) is False
    assert sc.file_common.is_file_exist(dst_file_path) is True
    with open(dst_file_path, 'r') as file_object:
        file_content = file_object.read()
        assert content in file_content


def test_delete_file(request):
    init_unittest(request)
    content = "First line\n"
    src_file_path = create_file_with_content(content=content)
    sc.file_common.delete_file(src_file_path)
    assert sc.file_common.is_file_exist(src_file_path) is False


def test_delete_dir(request):
    init_unittest(request)
    dir_path = create_empty_dir()
    sc.file_common.delete_dir(dir_path)
    assert sc.file_common.is_dir_exist(dir_path) is False


def test_delete_files_from_dir_with_extension(request):
    init_unittest(request)
    src_file_path1 = sc.file_register.get_registered_file_path(prefix="unittest1", extension="log", subdir="unittest")
    src_file_path2 = sc.file_register.get_registered_file_path(prefix="unittest2", extension="log", subdir="unittest")
    src_file_path3 = sc.file_register.get_registered_file_path(prefix="unittest3", extension="txt", subdir="unittest")
    sc.file_common.create_dir(dir_path=os.path.dirname(src_file_path1))
    content = "First line\n"
    create_file_with_content(content=content, src_file_path=src_file_path1)
    create_file_with_content(content=content, src_file_path=src_file_path2)
    create_file_with_content(content=content, src_file_path=src_file_path3)
    assert sc.file_common.is_file_exist(src_file_path1) is True
    assert sc.file_common.is_file_exist(src_file_path2) is True
    assert sc.file_common.is_file_exist(src_file_path3) is True
    sc.file_common.delete_files_from_dir_with_extension(subdir=os.path.dirname(src_file_path1), extension="log")
    assert sc.file_common.is_file_exist(src_file_path1) is False
    assert sc.file_common.is_file_exist(src_file_path2) is False
    assert sc.file_common.is_file_exist(src_file_path3) is True


def test_count_lines_in_empty_file(request):
    init_unittest(request)
    src_file_path = create_file_with_content(content="")
    assert sc.file_common.count_lines_in_file(src_file_path) == 0


def test_count_lines_in_file(request):
    init_unittest(request)
    src_file_path = create_file_with_content(content="First line\nSecond line\nThird line\n")
    assert sc.file_common.count_lines_in_file(src_file_path) == 3


def test_is_message_in_file(request):
    init_unittest(request)
    src_file_path = create_file_with_content(content="First line\nSecond line\nThird line\n")
    assert sc.file_common.is_message_in_file(file_path=src_file_path, expected_message="Second line") is True
    assert sc.file_common.is_message_in_file(file_path=src_file_path, expected_message="Non exist line") is False


def test_count_message_occurance_in_file(request):
    init_unittest(request)
    src_file_path = create_file_with_content(content="First line\nSecond line\nSecond line\n")
    assert sc.file_common.count_message_occurance_in_file(file_path=src_file_path, expected_message="Second line") == 2
    assert sc.file_common.count_message_occurance_in_file(file_path=src_file_path, expected_message="Non exist line") == 0


def test_grep_in_subdir(request):
    init_unittest(request)
    src_file_path1 = sc.file_register.get_registered_file_path(prefix="unittest1", extension="log", subdir="unittest")
    src_file_path2 = sc.file_register.get_registered_file_path(prefix="unittest2", extension="log", subdir="unittest")
    src_file_path3 = sc.file_register.get_registered_file_path(prefix="unittest3", extension="txt", subdir="unittest")
    sc.file_common.create_dir(dir_path=os.path.dirname(src_file_path1))
    content = "First line\n"
    create_file_with_content(content=content, src_file_path=src_file_path1)
    create_file_with_content(content=content, src_file_path=src_file_path2)
    create_file_with_content(content=content, src_file_path=src_file_path3)
    assert sc.file_common.grep_in_subdir(pattern="Non exist line", subdir=os.path.dirname(src_file_path1), extension="log") == []
    assert sc.file_common.grep_in_subdir(pattern="First line", subdir=os.path.dirname(src_file_path1), extension="log") == sorted([src_file_path1, src_file_path2])


def test_wait_for_messages_in_file(request):
    init_unittest(request)
    content = "First line\n"
    src_file_path = create_file_with_content(content=content)
    assert sc.file_common.wait_for_messages_in_file(file_path=src_file_path, expected_message=content, expected_occurance=1) is True
    assert sc.file_common.wait_for_messages_in_file(file_path=src_file_path, expected_message="Non exist line", expected_occurance=0) is True


def test_wait_for_lines_in_file(request):
    init_unittest(request)
    content = "First line\n"
    src_file_path = create_file_with_content(content=content)
    assert sc.file_common.wait_for_one_line_in_file(file_path=src_file_path, expected_line=1) is True
