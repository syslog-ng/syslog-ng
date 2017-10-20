import os

from source.testdb.initializer.setup_classes import SetupClasses


def init_unittest(request):
    sc = SetupClasses(request)
    return sc


def test_get_registered_file_path(request):
    sc = init_unittest(request)
    prefix = "unittest"
    extension = "log"
    sc.file_register.get_registered_file_path(prefix, extension)
    assert sc.file_register.registered_dirs == {}
    assert sc.file_register.registered_file_names == {}
    assert prefix in sc.file_register.registered_files.keys()
    assert sc.file_register.registered_files[prefix].startswith(
        os.path.join(sc.testdb_path_database.testcase_working_dir, prefix))
    assert sc.file_register.registered_files[prefix].endswith(extension)


def test_get_registered_dir_path(request):
    sc = init_unittest(request)
    prefix = "unittest"
    sc.file_register.get_registered_dir_path(prefix)
    assert sc.file_register.registered_files == {}
    assert sc.file_register.registered_file_names == {}
    assert prefix in sc.file_register.registered_dirs.keys()
    assert sc.file_register.registered_dirs[prefix].startswith(
        os.path.join(sc.testdb_path_database.testcase_working_dir, prefix))


def test_get_registered_file_name(request):
    sc = init_unittest(request)
    prefix = "unittest"
    extension = "log"
    sc.file_register.get_registered_file_name(prefix, extension)
    assert sc.file_register.registered_files == {}
    assert sc.file_register.registered_dirs == {}
    assert prefix in sc.file_register.registered_file_names.keys()
    assert sc.file_register.registered_file_names[prefix].startswith(prefix)
    assert sc.file_register.registered_file_names[prefix].endswith(extension)


def test_get_already_registered_file_path(request):
    sc = init_unittest(request)
    prefix = "unittest"
    extension = "log"
    sc.file_register.get_registered_file_path(prefix, extension)
    sc.file_register.get_registered_file_path(prefix, extension)
    assert sc.file_register.registered_dirs == {}
    assert sc.file_register.registered_file_names == {}
    assert prefix in sc.file_register.registered_files.keys()
    assert len(sc.file_register.registered_files.keys()) == 1
    assert sc.file_register.registered_files[prefix].startswith(
        os.path.join(sc.testdb_path_database.testcase_working_dir, prefix))
    assert sc.file_register.registered_files[prefix].endswith(extension)


def test_get_already_registered_dir_path(request):
    sc = init_unittest(request)
    prefix = "unittest"
    sc.file_register.get_registered_dir_path(prefix)
    sc.file_register.get_registered_dir_path(prefix)
    assert sc.file_register.registered_files == {}
    assert sc.file_register.registered_file_names == {}
    assert prefix in sc.file_register.registered_dirs.keys()
    assert len(sc.file_register.registered_dirs.keys()) == 1
    assert sc.file_register.registered_dirs[prefix].startswith(
        os.path.join(sc.testdb_path_database.testcase_working_dir, prefix))


def test_get_already_registered_file_name(request):
    sc = init_unittest(request)
    prefix = "unittest"
    extension = "log"
    sc.file_register.get_registered_file_name(prefix, extension)
    sc.file_register.get_registered_file_name(prefix, extension)
    assert sc.file_register.registered_files == {}
    assert sc.file_register.registered_dirs == {}
    assert prefix in sc.file_register.registered_file_names.keys()
    assert len(sc.file_register.registered_file_names.keys()) == 1
    assert sc.file_register.registered_file_names[prefix].startswith(prefix)
    assert sc.file_register.registered_file_names[prefix].endswith(extension)
