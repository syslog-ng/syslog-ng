# from src.registers.file_register import FileRegister
# from src.registers.port_register import PortRegister
# from src.syslog_ng.configuration.config import SyslogNgConfig
#
#
# def test_generate_config():
#     file_register = FileRegister()
#     port_register = PortRegister()
#     config = SyslogNgConfig(file_register=file_register, port_register=port_register)
#     src_options = {"file_path": "/tmp/input"}
#     dst_options = {"file_path": "/tmp/output"}
#     src_group = config.add_source_driver("file", src_options)
#     dst_group = config.add_destination_driver("file", dst_options)
#     config.add_logpath([src_group], [dst_group])
#     config.write_config()
#
#
# def test_auto_config():
#     file_register = FileRegister()
#     port_register = PortRegister()
#     config = SyslogNgConfig(file_register=file_register, port_register=port_register)
#     config.generate_config_with_drivers(source_driver="file", destination_driver="file")
#
#
# def test_empty_option():
#     file_register = FileRegister()
#     port_register = PortRegister()
#     config = SyslogNgConfig(file_register=file_register, port_register=port_register)
#     dst_options = {"file_path": "/tmp/output"}
#     src_group = config.add_source_driver("file", auto_mandatory_option=False)
#     dst_group = config.add_destination_driver("file", dst_options)
#     config.add_logpath([src_group], [dst_group])
#     config.write_config()
#
#
# def test_get_file_path_for_driver():
#     file_register = FileRegister()
#     port_register = PortRegister()
#     config = SyslogNgConfig(file_register=file_register, port_register=port_register)
#     config.config = {
#         "source_groups": [{
#             "group_name": "aaaa",
#             "drivers": [
#                 {
#                     "driver_name": "file",
#                     "driver_options": {"file_path": "/tmp/output"}
#                 }
#             ]
#         }
#
#         ]
#     }
#     assert config.get_file_path_for_group(group_type="source", group_name="aaaa", driver_name="file") == "/tmp/output"
#
#
# def test_socket_path_for_driver():
#     pass
