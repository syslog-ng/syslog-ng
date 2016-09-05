from src.common.run_command import run_command_for_exit_code, run_command_for_output
from src.syslog_ng.syslog_ng_path.path import SyslogNgPath


class SyslogNgCtl(object):
    def __init__(self):
        self.syslog_ng_path = SyslogNgPath()

    def stats(self):
        command = "%s stats --control=%s" % (self.syslog_ng_path.get_syslog_ng_control_tool(),
                                             self.syslog_ng_path.get_syslog_ng_control_socket())
        return run_command_for_output(command)

    def stats_reset(self):
        command = "%s stats --reset --control=%s" % (self.syslog_ng_path.get_syslog_ng_control_tool(),
                                                     self.syslog_ng_path.get_syslog_ng_control_socket())
        return run_command_for_output(command)

    def reload(self):
        command = "%s reload --control=%s" % (self.syslog_ng_path.get_syslog_ng_control_tool(),
                                              self.syslog_ng_path.get_syslog_ng_control_socket())
        return run_command_for_exit_code(command) == 0

    def stop(self):
        command = "%s stop --control=%s" % (self.syslog_ng_path.get_syslog_ng_control_tool(),
                                            self.syslog_ng_path.get_syslog_ng_control_socket())
        return run_command_for_exit_code(command) == 0

    def is_syslog_ng_running(self):
        command = "%s stats --control=%s" % (self.syslog_ng_path.get_syslog_ng_control_tool(),
                                             self.syslog_ng_path.get_syslog_ng_control_socket())
        return run_command_for_exit_code(command) == 0
