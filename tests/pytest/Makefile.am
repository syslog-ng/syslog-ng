EXTRA_DIST	+= \
		./bash_scripts/check_for_code_duplication.sh \
		./bash_scripts/create_class_diagram.sh \
		./bash_scripts/create_or_update_syslog_ng_ose_test_envs.sh \
		./bash_scripts/find_unused_functions.sh \
		./bash_scripts/run_callgraph_on_selftest.sh \
		./bash_scripts/run_coverage_on_selftest.sh \
		./bash_scripts/run_coverage_on_slng_code_base_when_running_testdb.sh \
		./bash_scripts/run_profiler_on_selftest.sh \
		./bash_scripts/run_profiler_on_syslog_ng_test.sh \
		./func_tests/generate_tests_for_options.py \
		./func_tests/__init__.py \
		./func_tests/ose_sandbox/__init__.py \
		./func_tests/ose_sandbox/ssl.crt \
		./func_tests/ose_sandbox/ssl.key \
		./func_tests/ose_sandbox/test_1.py \
		./func_tests/parser_tests/__init__.py \
		./func_tests/parser_tests/setup_parser.py \
		./func_tests/parser_tests/test_database.py \
		./func_tests/parser_tests/test_default_acd.py \
		./func_tests/testcase.py \
		./run_tests.py \
		./src/__init__.py \
		./src/options_and_drivers/db_drivers.py \
		./src/options_and_drivers/db_parser_options.py \
		./src/options_and_drivers/driver_data_provider.py \
		./src/options_and_drivers/__init__.py \
		./src/options_and_drivers/option_data_provider.py \
		./src/send_receive_logs/__init__.py \
		./src/send_receive_logs/log_generator/__init__.py \
		./src/send_receive_logs/log_generator/log_generator.py \
		./src/send_receive_logs/log_receiver/__init__.py \
		./src/send_receive_logs/log_receiver/log_receiver.py \
		./src/send_receive_logs/log_sender/__init__.py \
		./src/send_receive_logs/log_sender/log_sender.py \
		./src/syslog_ng/binary_syslog_ng/binary_syslog_ng.py \
		./src/syslog_ng/binary_syslog_ng_ctl/binary_syslog_ng_ctl.py \
		./src/syslog_ng/binary_syslog_ng_ctl/__init__.py \
		./src/syslog_ng/binary_syslog_ng_ctl/syslog_ng_ctl_utils.py \
		./src/syslog_ng/binary_syslog_ng/__init__.py \
		./src/syslog_ng/binary_syslog_ng/syslog_ng_utils.py \
		./src/syslog_ng/config_generator/config_generator.py \
		./src/syslog_ng/config_generator/config_template.pystache \
		./src/syslog_ng/config_generator/__init__.py \
		./src/syslog_ng/config_generator/selftest/__init__.py \
		./src/syslog_ng/config_generator/selftest/test_config_generator.py \
		./src/syslog_ng/__init__.py \
		./src/syslog_ng/path_handler/__init__.py \
		./src/syslog_ng/path_handler/path_handler.py \
		./src/testrun/__init__.py \
		./src/testrun/testrun_configuration_reader/__init__.py \
		./src/testrun/testrun_configuration_reader/testrun_configuration_reader.py \
		./src/testrun/testrun_reporter/__init__.py \
		./src/testrun/testrun_reporter/testrun_reporter.py \
		./src/testrun/testrun_validator/__init__.py \
		./src/testrun/testrun_validator/testrun_validator.py \
		./src/work_with_files/file_based_processor.py \
		./src/work_with_files/__init__.py \
		./src/work_with_process/__init__.py \
		./src/work_with_process/run_command.py \
		./src/work_with_sockets/__init__.py \
		./src/work_with_sockets/network_based_processor.py

pytest:
	cd $(top_srcdir)/tests/pytest
	@./run_tests.py -f -t custom -i ${exec_prefix}

.PHONY: pytest
