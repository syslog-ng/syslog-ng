import os
import shlex
import psutil
import signal

from source.executor.process_common import ProcessCommon


class ProcessExecutor(object):
    def __init__(self, testdb_logger, testdb_path_database):
        self.log_writer = testdb_logger.set_logger("ProcessExecutor")
        self.testcase_working_dir = testdb_path_database.testcase_working_dir

        self.process_common = ProcessCommon(testdb_logger=testdb_logger)

    def start_process(self, command, stdout, stderr, external_tool=None):
        if external_tool:
            command = self.set_external_tool_for_command(command=command, external_tool=external_tool)
        command_args = shlex.split(command)
        self.log_writer.info("Following process will be started: [%s]" % command)
        self.log_writer.debug("LD_LIBRARY_PATH: %s" % os.environ.get("LD_LIBRARY_PATH"))
        self.log_writer.debug("JAVA_HOME: %s" % os.environ.get("JAVA_HOME"))

        process = psutil.Popen(command_args, stderr=stderr, stdout=stdout)

        self.log_writer.info("Process started with pid [%s]" % process.pid)
        return process

    def reload_process(self, process):
        assert self.process_common.is_process_running(process=process) is True
        try:
            process.send_signal(signal.SIGHUP)
            self.log_writer.info("Process [%s] reloaded gracefully" % process.pid)
        except psutil.TimeoutExpired:
            self.log_writer.error("Process can not reloaded gracefully")
            process.send_signal(signal.SIGSEGV)
            assert False

    def stop_process(self, process):
        assert self.process_common.is_process_running(process=process) is True
        try:
            for child_process in process.children(recursive=True):
                child_process.send_signal(signal.SIGTERM)
            process.send_signal(signal.SIGTERM)
            exit_code = process.wait(timeout=2)
            self.log_writer.info("Process [%s] stopped gracefully with exit code [%s]" % (process.pid, exit_code))
            return exit_code
        except psutil.TimeoutExpired:
            self.log_writer.error("Process can not stopped gracefully")
            process.send_signal(signal.SIGSEGV)
            assert False

    @staticmethod
    def kill_process(process):
        process.send_signal(signal.SIGKILL)
        exit_code = process.wait(timeout=2)
        return exit_code

    def set_external_tool_for_command(self, command, external_tool):
        if external_tool == "strace":
            return self.use_command_with_strace(command=command)
        elif external_tool == "valgrind":
            return self.use_command_with_valgrind(command=command)
        elif external_tool == "perf":
            return self.use_command_with_perf(command=command)
        else:
            self.log_writer.error("Unknown external tool: %s" % external_tool)
            assert False

    def use_command_with_strace(self, command):
        strace_log_file = "strace.log"
        return "strace -s 8888 -ff -o %s %s" % (os.path.join(self.testcase_working_dir, strace_log_file), command)

    def use_command_with_valgrind(self, command):
        valgrind_log_file = "valgrind.log"
        return "valgrind --show-leak-kinds=all --track-origins=yes --tool=memcheck --leak-check=full --log-file=%s %s" % (os.path.join(self.testcase_working_dir, valgrind_log_file), command)

    def use_command_with_perf(self, command):
        perf_log_file = "perf.data"
        return "perf record -g -v -s --output=%s -F 99 %s" % (os.path.join(self.testcase_working_dir, perf_log_file), command)
