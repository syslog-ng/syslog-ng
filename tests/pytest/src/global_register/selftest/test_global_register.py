import re
from src.global_register.global_register import GlobalRegister

def test_get_uniq_tcp_port():
    gr = GlobalRegister()

    group_name = "abc_group_name"
    tcp_port = gr.get_uniq_tcp_port(group_name)

    group_name2 = "def_group_name"
    tcp_port2 = gr.get_uniq_tcp_port(group_name2)

    expected_registered_ports = {group_name: tcp_port, group_name2: tcp_port2}
    assert expected_registered_ports == gr.registered_tcp_ports

def test_get_uniq_udp_port():
    gr = GlobalRegister()

    group_name = "abc_group_name"
    udp_port = gr.get_uniq_udp_port(group_name)

    group_name2 = "def_group_name"
    udp_port2 = gr.get_uniq_udp_port(group_name2)

    expected_registered_ports = {group_name: udp_port, group_name2: udp_port2}
    assert expected_registered_ports == gr.registered_udp_ports


def test_get_uniq_filename():
    gr = GlobalRegister()

    prefix = "aaa"
    extension = "log"
    under_subdir = "micek"
    global_root_dir = "tmp"
    global_prefix = "pytest"
    expected_registered_files = []

    file_name = gr.get_uniq_filename(prefix=prefix, extension=extension, under_subdir=under_subdir)
    uniq_id_from_generated_file_name = file_name.split("_")[-1].split(".")[-2]
    # example file name: /tmp/micek/pytest_aaa_5be1cd4d-bf57-4433-b812-56593e9861ce.log

    expected_filename = "/%s/%s/%s_%s_%s.%s" % (global_root_dir, under_subdir, global_prefix, prefix, uniq_id_from_generated_file_name, extension)
    expected_registered_files.append(expected_filename)

    prefix2 = "bbb"
    extension2 = "conf"
    under_subdir2 = "micek2"

    file_name2 = gr.get_uniq_filename(prefix=prefix2, extension=extension2, under_subdir=under_subdir2)
    uniq_id_from_generated_file_name2 = file_name2.split("_")[-1].split(".")[-2]

    expected_filename2 = "/%s/%s/%s_%s_%s.%s" % (global_root_dir, under_subdir2, global_prefix, prefix2, uniq_id_from_generated_file_name2, extension2)
    expected_registered_files.append(expected_filename2)

    assert expected_registered_files == gr.registered_files

def test_get_uniq_dirname():
    gr = GlobalRegister()

    prefix = "config"
    prefix2 = "config2"
    global_root_dir = "tmp"
    global_prefix = "pytest"
    expected_registered_dirs = []

    dir_name = gr.get_uniq_dirname(prefix)
    # example dir name: /tmp/pytest_config_d8b001ef-de4d-4a76-b067-f903b5f45999
    uniq_id_from_generated_dir_name = dir_name.split("_")[-1]
    expected_dir_name = "/%s/%s_%s_%s" % (global_root_dir, global_prefix, prefix, uniq_id_from_generated_dir_name)

    dir_name2 = gr.get_uniq_dirname(prefix2)
    uniq_id_from_generated_dir_name2 = dir_name2.split("_")[-1]
    expected_dir_name2 = "/%s/%s_%s_%s" % (global_root_dir, global_prefix, prefix2, uniq_id_from_generated_dir_name2)

    expected_registered_dirs.append(expected_dir_name)
    expected_registered_dirs.append(expected_dir_name2)

    assert expected_registered_dirs == gr.registered_dirs