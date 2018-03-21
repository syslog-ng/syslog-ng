import subprocess
from source.executor.process_executor import ProcessExecutor
from source.executor.process_common import ProcessCommon
from source.executor.command_executor import CommandExecutor


class ExecutorInterface(object):
    def __init__(self, testdb_logger, testdb_path_database):
        self.testdb_logger = testdb_logger

        self.process_executor = ProcessExecutor(testdb_logger=testdb_logger, testdb_path_database=testdb_path_database)
        self.process_common = ProcessCommon(testdb_logger=testdb_logger)
        self.command_executor = CommandExecutor(testdb_logger=testdb_logger)

    def start_process(self, command, stdout=subprocess.PIPE, stderr=subprocess.PIPE, external_tool=None):
        return self.process_executor.start_process(command=command, stdout=stdout, stderr=stderr, external_tool=external_tool)

    def stop_process(self, process):
        return self.process_executor.stop_process(process=process)

    def reload_process(self, process):
        return self.process_executor.reload_process(process=process)

    def execute_command(self, command):
        return self.command_executor.execute_command(command=command)

    def is_pid_in_process_list(self, pid):
        return self.process_common.is_pid_in_process_list(pid=pid)

    def is_process_running(self, process):
        return self.process_common.is_process_running(process=process)

    def send_signal_to_process_with_name(self, process_name, process_signal):
        return self.process_common.send_signal_to_process_with_name(process_name=process_name, process_signal=process_signal)

    def get_pid_for_process_name(self, process_name=None, substring=None):
        return self.process_common.get_pid_for_process_name(process_name=process_name, substring=substring)

    def send_signal_to_process(self, process, process_signal):
        return self.process_common.send_signal_to_process(process=process, process_signal=process_signal)

    def dump_process_information(self, process):
        return self.process_common.dump_process_information(process=process)
