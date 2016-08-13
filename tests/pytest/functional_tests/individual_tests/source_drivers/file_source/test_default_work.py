from functional_tests.common.setup_and_teardown import *

def test_egyedi(setup_testcase):

    testcase = setup_testcase
    src_file = testcase.global_register.get_uniq_filename(prefix="src_file")
    dst_file = testcase.global_register.get_uniq_filename(prefix="dst_file")

    src_group = testcase.config.add_file_source(file_path=src_file)
    dst_group = testcase.config.add_file_destination(file_path=dst_file)
    testcase.config.render_config(logpath_management="auto")

    src_file_content = testcase.message_generator.generate_input_messages("file", "file", counter_start=1, counter_end=5)
    testcase.filemanager.create_file_with_content(file_path=src_file, content=src_file_content)

    expected_message2 = testcase.message_generator.generate_output_messages("file", "file", counter_start=1, counter_end=5)

    testcase.syslog_ng.start()
    file_content = testcase.filemanager.get_file_content(file_path=dst_file)
    assert file_content == expected_message2
    testcase.syslog_ng.stop()
