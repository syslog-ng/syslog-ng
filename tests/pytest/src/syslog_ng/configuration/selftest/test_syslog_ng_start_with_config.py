# import time
# from src.message.receiver.receiver import Receiver
# from src.message.sender.sender import IntuitiveSender
from src.syslog_ng.configuration.config import SyslogNgConfig
from src.syslog_ng.syslog_ng_binary.syslog_ng import SyslogNg
# from src.syslog_ng.syslog_ng_utils.utils import SyslogNgUtils
# from src.syslog_ng.syslog_ng_ctl.syslog_ng_ctl import SyslogNgCtl
# from src.drivers.socket.socket_reader import SocketReader

config = SyslogNgConfig.Instance()
slng = SyslogNg()
# sender = IntuitiveSender()
# receiver = Receiver()
# utils = SyslogNgUtils()
# ctl = SyslogNgCtl()
# s_reader = SocketReader.Instance()

# def test_slng_start():
#     src_group, dst_group = config.generate_config_with_drivers(source_driver="file", destination_driver="tcp")
#     # src_file = config.get_file_path_for_group(group_type="source", group_name=src_group, driver_name="file")
#     # dst_file = config.get_file_path_for_group(group_type="destination", group_name=dst_group, driver_name="file")
#     ip, port = config.get_socket_path_for_group(group_type="destination", group_name=dst_group, driver_name="tcp")
#     slng.start()
#     sender.send_message2(message="almafa\n")
#     # assert "almafa" in receiver.wait_for_message_in_file(file_path=dst_file)
#     receiver.wait_for_message_in_socket(ip=ip, port=port)
#     assert "almafa" in s_reader.get_messages_from_tcp(ip=ip, port=port)
#     slng.stop()

# def test_second():
#     config.generate_config_with_drivers(source_driver="file", destination_driver="file")
#     slng.start()
#     sender.send_message(message="almafa\n")
#     receiver.wait_for_messages()
#     slng.stop()
#     testrun.check_output()
#
# >slng.start ; megvarja a "starting up"-ot
# >sender.send_message ; megvarja , hogy a "stats processed a group-hoz ne nojon"
# >receiver.wait_for_messages ; megvarja hogy tobb ne jojjon a kimenetre
# >slng.stop ; megvarja a "shutting down"-t
# >testrun.check_output ; osszehasonlit

def test_minor():
    # config.generate_config_with_drivers(source_driver="file", destination_driver="tcp")
    src_group = config.add_source_driver(driver_name="file", options={"aaa": "vvv"})
    dst_group = config.add_destination_driver(driver_name="file", auto_mandatory_option=True)
    slng.start()
    slng.stop()
    assert "aa" == "vv"