import shlex
import psutil
import subprocess


class CommandExecutor(object):
    def __init__(self, testdb_logger):
        self.log_writer = testdb_logger.set_logger("CommandExecutor")

    def execute_command(self, command):
        command_args = shlex.split(command)
        self.log_writer.debug("Following command will be executed: [%s]" % command)
        with psutil.Popen(command_args, stderr=subprocess.PIPE, stdout=subprocess.PIPE) as proc:
            exit_code = proc.wait(timeout=10)
            stdout = str(proc.stdout.read(), 'utf-8')
            stderr = str(proc.stderr.read(), 'utf-8')
        self.log_writer.debug("Exit code: [%s]" % exit_code)
        self.log_writer.debug("stdout: [%s]" % stdout)
        self.log_writer.debug("stderr: [%s]" % stderr)
        return exit_code, stdout, stderr
