from functional_tests.common.setup_and_teardown import *
from src.work_with_sockets.socket_sender import SocketSender
from src.work_with_sockets.socket_listener import SocketListener

def test_egyedi(setup_testcase):

    sender = SocketSender()
    listener = SocketListener()
    testcase = setup_testcase
    src_port = testcase.global_register.get_uniq_tcp_port()
    dst_port = testcase.global_register.get_uniq_tcp_port()

    testcase.config.add_global_options({"keep_hostname": "yes"})
    src_group = testcase.config.add_tcp_source(options={"ip": "127.0.0.1", "port": src_port})
    dst_group = testcase.config.add_tcp_destination(options={"ip": "@@127.0.0.1@@", "port": dst_port})
    testcase.config.render_config(logpath_management="auto")
    print(testcase.config.get_config())

    #
    # src_file_content = testcase.message_generator.generate_input_messages("file", "file", counter_start=1, counter_end=5)
    # testcase.filemanager.create_file_with_content(file_path=src_file, content=src_file_content)
    #
    # expected_message2 = testcase.message_generator.generate_output_messages("file", "file", counter_start=1, counter_end=5)
    #
    listener.tcp_listener("127.0.0.1", dst_port)
    testcase.syslog_ng.start()
    sender.tcp_send("127.0.0.1", src_port, testcase.message_generator.generate_input_messages("tcp", "tcp"))
    # # file_content = testcase.filemanager.get_file_content(file_path=dst_file)
    received_messages = listener.get_received_messages()['tcp']
    # # assert file_content == expected_message2
    testcase.syslog_ng.stop()
    # listener.tcp_listener_stop()
    expected_message = testcase.message_generator.generate_output_messages("tcp", "tcp", with_new_line=True)
    assert expected_message == received_messages
