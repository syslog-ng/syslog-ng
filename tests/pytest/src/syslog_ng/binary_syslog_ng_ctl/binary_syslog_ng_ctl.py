from src.syslog_ng.path_handler.path_handler import SyslogNgPathHandler
from src.work_with_process import run_command

class SyslogNgCtl(SyslogNgPathHandler):
    def __init__(self):
        SyslogNgPathHandler.__init__(self)
        self.global_config = None

    def set_global_config(self, global_config):
        self.global_config = global_config

    def stats(self):
        command = "%s stats --control=%s" % (self.get_syslog_ng_control_tool(), self.get_syslog_ng_control_socket())
        return run_command.run_command_for_output(command)

    def stats_reset(self):
        command = "%s stats --reset --control=%s" % (self.get_syslog_ng_control_tool(), self.get_syslog_ng_control_socket())
        return run_command.run_command_for_output(command)

    def reload(self):
        command = "%s reload --control=%s" % (self.get_syslog_ng_control_tool(), self.get_syslog_ng_control_socket())
        return run_command.run_command_for_exit_code(command) == 0

    def stop(self):
        command = "%s stop --control=%s" % (self.get_syslog_ng_control_tool(), self.get_syslog_ng_control_socket())
        return run_command.run_command_for_exit_code(command) == 0

    def is_syslog_ng_running(self):
        command = "%s stats --control=%s" % (self.get_syslog_ng_control_tool(), self.get_syslog_ng_control_socket())
        return run_command.run_command_for_exit_code(command) == 0
