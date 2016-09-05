# from src.drivers.socket.socket_reader import SocketReader
# from src.drivers.socket.socket_writer import SocketWriter
#
#
# def test_a():
#     s_reader = SocketReader()
#     s_writer = SocketWriter()
#     dest_group = "agroup"
#     test_message = "test aaaa\ntest vvvv"
#
#     s_reader.wait_for_messages_on_tcp(group=dest_group)
#     s_writer.send_message_to_tcp(group=dest_group, message=test_message)
#
#     assert s_reader.get_messages_from_tcp(group=dest_group) == test_message
