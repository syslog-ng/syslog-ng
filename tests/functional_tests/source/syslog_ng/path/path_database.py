import os


class SyslogNgPathDatabase(object):
    def __init__(self, testdb_logger, testdb_path_database, testdb_config_context, file_register, topology):
        self.log_writer = testdb_logger.set_logger("SyslogNgPath")
        self.testdb_config_context = testdb_config_context
        self.testdb_path_database = testdb_path_database
        self.file_register = file_register
        self.topology = topology

        self.balabitpkg_install_path = "/opt/syslog-ng"

    @property
    def syslog_ng_binary_files(self):
        syslog_ng_install_path = ""
        if self.testdb_config_context.syslog_ng_install_mode == "custom":
            syslog_ng_install_path = self.testdb_config_context.syslog_ng_install_path
            if not syslog_ng_install_path:
                self.log_writer.error("There is no syslog-ng install path defined: [%s]" % syslog_ng_install_path)
                assert False
        return {
            "syslog-ng": {
                "path": "syslog-ng",
                "osepkg": "/usr/sbin/syslog-ng",
                "balabitpkg": os.path.join(self.balabitpkg_install_path, "sbin/syslog-ng"),
                "custom": os.path.join(syslog_ng_install_path, "sbin/syslog-ng"),
            },
            "syslog-ng-ctl": {
                "path": "syslog-ng-ctl",
                "osepkg": "/usr/sbin/syslog-ng-ctl",
                "balabitpkg": os.path.join(self.balabitpkg_install_path, "sbin/syslog-ng-ctl"),
                "custom": os.path.join(syslog_ng_install_path, "sbin/syslog-ng-ctl"),
            },
            "loggen": {
                "path": "loggen",
                "osepkg": "/usr/sbin/loggen",
                "balabitpkg": os.path.join(self.balabitpkg_install_path, "bin/loggen"),
                "custom": os.path.join(syslog_ng_install_path, "bin/loggen"),
            },
            "dqtool": {
                "path": "dqtool",
                "osepkg": "/usr/sbin/dqtool",
                "balabitpkg": os.path.join(self.balabitpkg_install_path, "bin/dqtool"),
                "custom": os.path.join(syslog_ng_install_path, "bin/dqtool"),
            },
            "pdbtool":  {
                "path": "pdbtool",
                "osepkg": "/usr/sbin/pdbtool",
                "balabitpkg": os.path.join(self.balabitpkg_install_path, "bin/pdbtool"),
                "custom": os.path.join(syslog_ng_install_path, "bin/pdbtool"),
            }
        }

# binary files
    def get_syslog_ng_binary_path(self):
        syslog_ng_install_mode = self.testdb_config_context.syslog_ng_install_mode
        return self.syslog_ng_binary_files['syslog-ng'][syslog_ng_install_mode]

    def get_syslog_ng_control_tool_path(self):
        syslog_ng_install_mode = self.testdb_config_context.syslog_ng_install_mode
        return self.syslog_ng_binary_files['syslog-ng-ctl'][syslog_ng_install_mode]

    def get_loggen_path(self):
        syslog_ng_install_mode = self.testdb_config_context.syslog_ng_install_mode
        return self.syslog_ng_binary_files['loggen'][syslog_ng_install_mode]

    def get_dqtool_path(self):
        syslog_ng_install_mode = self.testdb_config_context.syslog_ng_install_mode
        return self.syslog_ng_binary_files['dqtool'][syslog_ng_install_mode]

    def get_pdbtool_path(self):
        syslog_ng_install_mode = self.testdb_config_context.syslog_ng_install_mode
        return self.syslog_ng_binary_files['pdbtool'][syslog_ng_install_mode]

# socket files
    def get_control_socket_path(self):
        return self.file_register.get_registered_file_path(prefix="control_socket_%s" % self.topology, extension="ctl")

# config files
    def get_config_path(self):
        return self.file_register.get_registered_file_path(prefix="config_%s" % self.topology, extension="conf")

# log files
    def get_internal_log_path(self):
        return self.file_register.get_registered_file_path(prefix="dst_file_internal_%s" % self.topology, extension="txt")

    def get_stdout_log_path(self):
        return self.file_register.get_registered_file_path(prefix="stdout_%s" % self.topology, extension="log")

    def get_stderr_log_path(self):
        return self.file_register.get_registered_file_path(prefix="stderr_%s" % self.topology, extension="log")

# other files
    def get_pid_path(self):
        return self.file_register.get_registered_file_path(prefix="pid_%s" % self.topology, extension="pid")

    def get_persist_state_path(self):
        return self.file_register.get_registered_file_path(prefix="persist_%s" % self.topology, extension="persist")

    def get_core_file_path(self):
        testdb_root_dir = self.testdb_path_database.testdb_root_dir
        core_file_pattern = [os.path.join("/tmp", "core"), os.path.join(testdb_root_dir, "core")]
        return core_file_pattern

    def get_java_core_file_path(self, pid):
        testdb_root_dir = self.testdb_path_database.testdb_root_dir
        java_core_file_pattern = os.path.join(testdb_root_dir, "hs_err_pid%s.log" % pid)
        return java_core_file_pattern
