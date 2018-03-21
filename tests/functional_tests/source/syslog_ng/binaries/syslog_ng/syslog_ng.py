import binascii
import os
import shutil

from source.testdb.common.common import wait_till_function_not_false, wait_till_function_not_true


class SyslogNg(object):
    def __init__(self, testdb_logger, testdb_path_database, testcase_name, file_common, file_listener, executor, syslog_ng_path_database, syslog_ng_ctl):
        self.syslog_ng_path_database = syslog_ng_path_database
        self.file_helper = file_common
        self.executor = executor
        self.testdb_logger = testdb_logger
        self.log_writer = testdb_logger.set_logger("SyslogNg")
        self.testdb_path_database = testdb_path_database
        self.syslog_ng_ctl = syslog_ng_ctl
        self.file_listener = file_listener
        self.testcase_name = testcase_name

        self.expected_run = None
        self.external_tool = None
        self.syslog_ng_process = None
        self.syslog_ng_pid = None
        self.registered_start = 0
        self.registered_stop = 0
        self.registered_reload = 0
        self.stdout_fd = None
        self.stderr_fd = None

# properties
    @property
    def syslog_ng_start_message(self):
        return ["syslog-ng starting up;"]

    @property
    def syslog_ng_stop_message(self):
        return ["syslog-ng shutting down"]

    @property
    def syslog_ng_reload_messages(self):
        return ["New configuration initialized", "Configuration reload request received, reloading configuration", "Configuration reload finished"]

# high level functions
    def start(self, external_tool=None, expected_run=True):
        self.expected_run = expected_run
        self.external_tool = external_tool

        os.environ["LD_LIBRARY_PATH"] = self.testdb_path_database.libjvm_dir
        exit_code, stdout, stderr = self.executor.execute_command(
            command=self.get_syslog_ng_start_command(syntax_check=True)
        )
        self.handle_config_syntax_check_result(exit_code, stdout, stderr)

        if not self.stdout_fd:
            self.stdout_fd = open(self.syslog_ng_path_database.get_stdout_log_path(), 'w')
            self.stderr_fd = open(self.syslog_ng_path_database.get_stderr_log_path(), 'w')
        syslog_ng_process = self.executor.start_process(
            command=self.get_syslog_ng_start_command(syntax_check=False),
            stdout=self.stdout_fd,
            stderr=self.stderr_fd,
            external_tool=external_tool
        )
        self.syslog_ng_process = syslog_ng_process
        self.syslog_ng_pid = syslog_ng_process.pid
        if not self.external_tool:
            syslog_ng_start_result = self.wait_for_syslog_ng_start()
            self.handle_syslog_ng_start_process_result(syslog_ng_start_result=syslog_ng_start_result)
        else:
            self.syslog_ng_ctl.wait_for_control_socket_start()
            wait_till_function_not_true(self.executor.is_pid_in_process_list, self.get_syslog_ng_pid(), monitoring_time=1)
        return True

    def stop(self):
        if (not self.external_tool) and (not self.executor.is_pid_in_process_list(pid=self.get_syslog_ng_pid())):
            self.log_writer.error("syslog-ng was killed outside of syslog-ng handler. last pid: [%s]" % self.syslog_ng_pid)
            self.syslog_ng_process = None
            self.syslog_ng_pid = None
            self.is_core_file_exist()
            assert False
        self.executor.stop_process(process=self.get_syslog_ng_process())
        self.wait_for_syslog_ng_stop()
        self.handle_syslog_ng_stop_process_result()
        return True

    def reload(self):
        if (not self.external_tool) and (not self.executor.is_pid_in_process_list(pid=self.get_syslog_ng_pid())):
            self.log_writer.error("syslog-ng was killed outside of syslog-ng handler. last pid: [%s]" % self.syslog_ng_pid)
            self.syslog_ng_process = None
            self.syslog_ng_pid = None
            self.is_core_file_exist()
            assert False
        self.executor.reload_process(process=self.get_syslog_ng_process())
        self.wait_for_syslog_ng_reload()
        self.handle_syslog_ng_reload_process_result()
        return True

    def restart(self):
        self.stop()
        self.start()

# Low level functions
    def get_syslog_ng_start_command(self, syntax_check=None):
        start_command = "%s " % self.syslog_ng_path_database.get_syslog_ng_binary_path()
        start_command += "-Fedtv --no-caps --enable-core "
        if syntax_check:
            start_command += "--syntax-only "
        start_command += "-f %s " % self.syslog_ng_path_database.get_config_path()
        start_command += "-R %s " % self.syslog_ng_path_database.get_persist_state_path()
        start_command += "-p %s " % self.syslog_ng_path_database.get_pid_path()
        start_command += "-c %s " % self.syslog_ng_path_database.get_control_socket_path()
        return start_command

    def handle_config_syntax_check_result(self, exit_code, stdout, stderr):
        if self.expected_run and (exit_code != 0):
            self.log_writer.error("syslog-ng can not start with config.\nExit code: [%s], \nStdout: [%s], \nStderr: [%s]" % (exit_code, stdout, stderr))
            assert False
        elif not self.expected_run and (exit_code != 0):
            self.log_writer.info("syslog-ng can not started, but this was the expected behaviour")
        else:
            self.log_writer.info("STEP syslog-ng can started with config")

    def handle_syslog_ng_start_process_result(self, syslog_ng_start_result):
        if not self.expected_run and (not syslog_ng_start_result):
            self.log_writer.info("syslog-ng can not started, but this was the expected behaviour")
        elif (not syslog_ng_start_result) or (not self.executor.is_pid_in_process_list(pid=self.get_syslog_ng_pid())):
            self.file_listener.dump_file_content(file_path=self.syslog_ng_path_database.get_stdout_log_path(), message_level="error")
            self.file_listener.dump_file_content(file_path=self.syslog_ng_path_database.get_stderr_log_path(), message_level="error")
            self.log_writer.error("syslog-ng can not started successfully")
            assert False
        else:
            self.log_writer.info("STEP syslog-ng started successfully")

    def handle_syslog_ng_stop_process_result(self):
        if self.executor.is_pid_in_process_list(pid=self.get_syslog_ng_pid()):
            self.file_listener.dump_file_content(file_path=self.syslog_ng_path_database.get_stdout_log_path(), message_level="error")
            self.file_listener.dump_file_content(file_path=self.syslog_ng_path_database.get_stderr_log_path(), message_level="error")
            self.log_writer.error("syslog-ng can not stopped successfully")
            assert False
        else:
            self.syslog_ng_process = None
            self.syslog_ng_pid = None
            self.log_writer.info("syslog-ng stopped successfully")

    def handle_syslog_ng_reload_process_result(self):
        if not self.executor.is_pid_in_process_list(pid=self.get_syslog_ng_pid()):
            self.file_listener.dump_file_content(file_path=self.syslog_ng_path_database.get_stdout_log_path(), message_level="error")
            self.file_listener.dump_file_content(file_path=self.syslog_ng_path_database.get_stderr_log_path(), message_level="error")
            self.log_writer.error("syslog-ng can not reloaded successfully")
            assert False
        else:
            self.log_writer.info("syslog-ng reloaded successfully")

    def wait_for_syslog_ng_start(self):
        control_socket_alive = False
        pid_in_process_list = False

        self.registered_start += 1
        start_message_arrived = self.wait_for_expected_syslog_ng_messages(messages=self.syslog_ng_start_message, expected_occurance=self.registered_start)
        self.testdb_logger.write_message_based_on_value(logsource=self.log_writer, message="syslog-ng start message arrived", value=start_message_arrived)
        if start_message_arrived:
            control_socket_alive = self.syslog_ng_ctl.wait_for_control_socket_start()
            self.testdb_logger.write_message_based_on_value(logsource=self.log_writer, message="syslog-ng control socket alive", value=control_socket_alive)
        if control_socket_alive:
            pid_in_process_list = wait_till_function_not_true(self.executor.is_pid_in_process_list, self.get_syslog_ng_pid(), monitoring_time=1)
            self.testdb_logger.write_message_based_on_value(logsource=self.log_writer, message="syslog-ng's pid is in process list", value=pid_in_process_list)
        self.is_core_file_exist()
        return start_message_arrived and control_socket_alive and pid_in_process_list

    def wait_for_syslog_ng_stop(self):
        control_socket_not_alive = False
        pid_not_in_process_list = False

        self.registered_stop += 1
        stop_message_arrived = self.wait_for_expected_syslog_ng_messages(messages=self.syslog_ng_stop_message, expected_occurance=self.registered_stop)
        self.testdb_logger.write_message_based_on_value(logsource=self.log_writer, message="syslog-ng stop message arrived", value=stop_message_arrived)
        if stop_message_arrived:
            control_socket_not_alive = self.syslog_ng_ctl.wait_for_control_socket_stop()
            self.testdb_logger.write_message_based_on_value(logsource=self.log_writer, message="syslog-ng control socket not alive", value=control_socket_not_alive)
        if control_socket_not_alive:
            pid_not_in_process_list = wait_till_function_not_false(self.executor.is_pid_in_process_list, self.get_syslog_ng_pid())
            self.testdb_logger.write_message_based_on_value(logsource=self.log_writer, message="syslog-ng's pid is not in process list", value=pid_not_in_process_list)
        self.is_core_file_exist()
        return stop_message_arrived and control_socket_not_alive and pid_not_in_process_list

    def wait_for_syslog_ng_reload(self):
        control_socket_alive = False
        pid_in_process_list = False

        self.registered_reload += 1
        reload_message_arrived = self.wait_for_expected_syslog_ng_messages(messages=self.syslog_ng_reload_messages, expected_occurance=self.registered_reload)
        self.testdb_logger.write_message_based_on_value(logsource=self.log_writer, message="syslog-ng reload message arrived", value=reload_message_arrived)
        if reload_message_arrived:
            control_socket_alive = self.syslog_ng_ctl.wait_for_control_socket_start()
            self.testdb_logger.write_message_based_on_value(logsource=self.log_writer, message="syslog-ng control socket alive", value=control_socket_alive)
        if control_socket_alive:
            pid_in_process_list = wait_till_function_not_true(self.executor.is_pid_in_process_list, self.get_syslog_ng_pid(), monitoring_time=1)
            self.testdb_logger.write_message_based_on_value(logsource=self.log_writer, message="syslog-ng's pid is in process list", value=pid_in_process_list)
        self.is_core_file_exist()
        return reload_message_arrived and control_socket_alive and pid_in_process_list

    def wait_for_expected_syslog_ng_messages(self, messages, expected_occurance=1):
        result = []
        for message in messages:
            result.append(self.file_helper.wait_for_messages_in_file(file_path=self.syslog_ng_path_database.get_stderr_log_path(), expected_message=message, expected_occurance=expected_occurance))
        return all(result)

    def is_core_file_exist(self):
        for syslog_ng_core_file in self.syslog_ng_path_database.get_core_file_path():
            if self.file_helper.is_file_exist(file_path=syslog_ng_core_file):
                core_backtrace_filename = "core_%s_%s" % (self.testcase_name, binascii.b2a_hex(os.urandom(5)).decode('ascii'))
                core_backtrace_path = os.path.join(self.testdb_path_database.testcase_working_dir, core_backtrace_filename)
                gdb_command = "gdb --core %s %s -ex 'bt full' --batch" % (syslog_ng_core_file, self.syslog_ng_path_database.get_syslog_ng_binary_path())
                exit_code, output, error = self.executor.execute_command(command=gdb_command)
                with open(core_backtrace_path, 'w') as file_object:
                    file_object.write(output)
                shutil.move(syslog_ng_core_file, "%s/" % self.testdb_path_database.testcase_working_dir)
                if self.file_helper.is_file_exist(file_path=self.syslog_ng_path_database.get_java_core_file_path(pid=self.get_syslog_ng_pid())):
                    shutil.move(self.syslog_ng_path_database.get_java_core_file_path(pid=self.get_syslog_ng_pid()), "%s/" % self.testdb_path_database.testcase_working_dir)
                self.log_writer.error("core file detected")
                self.syslog_ng_process = None
                self.syslog_ng_pid = None
                assert False
        return True

    def get_syslog_ng_pid(self):
        return self.syslog_ng_pid

    def get_syslog_ng_process(self):
        return self.syslog_ng_process

    def is_concurent_syslog_running(self):
        concurent_syslog_ng_pid = self.executor.get_pid_for_process_name(process_name="syslog-ng")
        concurent_rsyslog_pid = self.executor.get_pid_for_process_name(process_name="rsyslog")
        if concurent_syslog_ng_pid or concurent_rsyslog_pid:
            raise SystemExit("Concurent syslogger already running, please stop it, syslog-ng.pid: [%s], rsyslog.pid: [%s]" % (
                concurent_syslog_ng_pid, concurent_rsyslog_pid), 1)
