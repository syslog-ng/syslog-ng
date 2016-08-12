from functional_tests.common.setup_and_teardown import *
# tc = TestCase()
#
# slng = tc.syslog-ng
# config = tc.config
# filemanager = tc.file_manager
# message_generator = tc.message_generator
# receiver = tc.receiver
# global_registry = tc.global_registry

def test_egyedi(setup_testcase):
    print("AAAAAAAAA")
    # src_file = global_registry.get_uniq_filepath()
    # dst_file = global_registry.get_uniq_filepath()
    # src_group = config.add_file_source(path=src_file, src_content="", options="")
    # dst_group = config.add_file_destination(path=dst_file)
    # config.render_config(logpath_management="auto")
    # # custom_logpath = [[src_group1, filter_group1, dest_group1], [src_group2, filter_group2, dest_group2]]
    # # config.render_config(logpath_management="custom", logpath=custom_logpath)
    # # config.connect_drivers(src=src_group, dst=dst_group)
    #
    # src_file_content = message_generator.get_bsd_message()
    # filemanager.create_file_with_content(group=src_file, content=src_file_content)
    # # filemanager.append_content_to_file(group=src_file, content=src_file_content)
    # expected_message = "aaa"
    #
    # slng.start()
    # assert filemanager.get_file_content(path=dst_file) == expected_message
    # slng.stop()
