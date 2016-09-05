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
./func_tests/modules/__init__.py \
./func_tests/ose_sandbox/__init__.py \
./func_tests/ose_sandbox/test_1.py \
./functional_tests/common/__init__.py \
./functional_tests/common/setup_and_teardown.py \
./functional_tests/common/testcase.py \
./functional_tests/common/test_parametrization.py \
./functional_tests/individual_tests/destination/python/__init__.py \
./functional_tests/individual_tests/destination/python/sngtestmod.py \
./functional_tests/individual_tests/destination/python/test_ok_vp_file.py \
./functional_tests/individual_tests/__init__.py \
./functional_tests/individual_tests/source_drivers/file_source/__init__.py \
./functional_tests/individual_tests/source_drivers/file_source/test_default_work.py \
./functional_tests/individual_tests/source_drivers/__init__.py \
./functional_tests/individual_tests/source_drivers/tcp_source/__init__.py \
./functional_tests/individual_tests/source_drivers/tcp_source/test_default_work.py \
./functional_tests/__init__.py \
./functional_tests/option_related_tests/__init__.py \
./run_tests.py \
./src/common/__init__.py \
./src/common/run_command.py \
./src/common/singleton.py \
./src/common/waiters.py \
./src/drivers/file/file_driver.py \
./src/drivers/file/file_reader.py \
./src/drivers/file/file_writer.py \
./src/drivers/file/__init__.py \
./src/drivers/__init__.py \
./src/drivers/socket/__init__.py \
./src/drivers/socket/selftest/__init__.py \
./src/drivers/socket/selftest/test_socket_reader.py \
./src/drivers/socket/socket_driver.py \
./src/drivers/socket/socket_reader.py \
./src/drivers/socket/socket_writer.py \
./src/__init__.py \
./src/message/generator/generator.py \
./src/message/generator/__init__.py \
./src/message/__init__.py \
./src/message/receiver/__init__.py \
./src/message/receiver/receiver.py \
./src/message/sender/__init__.py \
./src/message/sender/sender.py \
./src/registers/file_register.py \
./src/registers/global_register.py \
./src/registers/__init__.py \
./src/registers/port_register.py \
./src/syslog_ng/configuration/config.py \
./src/syslog_ng/configuration/__init__.py \
./src/syslog_ng/configuration/selftest/__init__.py \
./src/syslog_ng/configuration/selftest/test_generator.py \
./src/syslog_ng/configuration/selftest/test_syslog_ng_start_with_config.py \
./src/syslog_ng/configuration/writer.py \
./src/syslog_ng/driver_database/__init__.py \
./src/syslog_ng/__init__.py \
./src/syslog_ng/option_database/__init__.py \
./src/syslog_ng/syslog_ng_binary/__init__.py \
./src/syslog_ng/syslog_ng_binary/syslog_ng.py \
./src/syslog_ng/syslog_ng_ctl/__init__.py \
./src/syslog_ng/syslog_ng_ctl/syslog_ng_ctl.py \
./src/syslog_ng/syslog_ng_path/__init__.py \
./src/syslog_ng/syslog_ng_path/path.py \
./src/syslog_ng/syslog_ng_utils/__init__.py \
./src/syslog_ng/syslog_ng_utils/utils.py \
./src/test_configuration/__init__.py \
./src/test_configuration/testrun_configuration.py

pytest:
	sh -c "\
   cd $(top_srcdir)/tests/pytest && \
   PATH=$PATH:${exec_prefix}/sbin \
   ./run_tests.py \
   "

.PHONY: pytest
