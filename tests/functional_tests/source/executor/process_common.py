import psutil


class ProcessCommon(object):
    def __init__(self, testdb_logger):
        self.log_writer = testdb_logger.set_logger("ProcessCommon")

    @staticmethod
    def is_process_running(process):
        return process.is_running()

    @staticmethod
    def is_pid_in_process_list(pid):
        return psutil.pid_exists(pid)

    @staticmethod
    def get_pid_for_process_name(process_name=None, substring=None):
        for proc in psutil.process_iter():
            if process_name and (process_name == proc.name()):
                return proc.pid
            if substring and (str(substring) in str(proc.cmdline())):
                return proc.pid

    @staticmethod
    def get_process_for_process_name(process_name=None, substring=None):
        for proc in psutil.process_iter():
            if process_name and (process_name == proc.name()):
                return proc
            if substring and (str(substring) in str(proc.cmdline())):
                return proc

    def dump_process_information(self, process):
        self.log_writer.info("As dict: [%s]" % str(process.as_dict()))

    def send_signal_to_process_with_name(self, process_name, process_signal):
        proc = self.get_process_for_process_name(process_name=process_name)
        proc.send_signal(process_signal)
        exit_code = proc.wait()
        return exit_code

    def send_signal_to_process(self, process, process_signal):
        process.send_signal(process_signal)
        exit_code = process.wait()
        return exit_code
