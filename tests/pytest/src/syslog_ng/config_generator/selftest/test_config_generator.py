from src.syslog_ng.config_generator.config_generator import ConfigGenerator


def test_add_global_options():
    config = ConfigGenerator()
    config.add_global_options({"keep_hostname": "yes"})

    expected_config = [{'option_name': 'keep_hostname', 'option_value': 'yes'}]
    assert expected_config == config.get_config()['global_options']

def test_get_internal_log_output_path():
    config = ConfigGenerator()
    config.config = {'version': '3.8', 'logpath': [{'sources': ['source_file_2eea8a9e9a'], 'destinations': ['destination_file_ddff290bdd']}, {'sources': ['source_internal_b6b9b5028c'], 'destinations': ['destination_file_1f10120b0e']}], 'destination_groups': [{'destination_group_name': 'destination_file_ddff290bdd', 'destination_drivers': [{'destination_driver_name': 'file', 'driver_options': [{'non_tls_options': [{'without_option_name': [{'option_name': 'file_path', 'option_value': '@@/tmp/destination_file_e3cbe383c2.txt@@'}]}]}]}]}, {'destination_group_name': 'destination_file_1f10120b0e', 'destination_drivers': [{'destination_driver_name': 'file', 'driver_options': [{'non_tls_options': [{'without_option_name': [{'option_name': 'file_path', 'option_value': '@@/tmp/destination_file_5a594275f7.txt@@'}]}]}]}]}], 'global_options': [{'option_name': 'keep_hostname', 'option_value': 'yes'}], 'source_groups': [{'source_group_name': 'source_file_2eea8a9e9a', 'source_drivers': [{'driver_options': [{'non_tls_options': [{'without_option_name': [{'option_name': 'file_path', 'option_value': '@@/tmp/source_file_a1b89c3c07.txt@@'}]}]}], 'source_driver_name': 'file'}]}, {'source_group_name': 'source_internal_b6b9b5028c', 'source_drivers': [{'driver_options': [{'non_tls_options': []}], 'source_driver_name': 'internal'}]}], 'parser_groups': []}
    expected_internal_log_path = "/tmp/destination_file_5a594275f7.txt"

    assert expected_internal_log_path == config.get_internal_log_output_path()