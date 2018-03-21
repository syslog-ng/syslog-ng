class TestdbConfigContext(object):
    def __init__(self, testdb_path_database, testcase_context):
        self.testdb_path_database = testdb_path_database
        self.testcase_context = testcase_context

        self.__create_testcase_config()

    @property
    def syslog_ng_install_path(self):
        return self.testcase_context.getfixturevalue("installpath")

    @property
    def syslog_ng_install_mode(self):
        return self.testcase_context.getfixturevalue("installmode")

    @property
    def syslog_ng_version(self):
        return self.testcase_context.getfixturevalue("syslogngversion")

    @property
    def log_level(self):
        return self.testcase_context.getfixturevalue("loglevel")

    @property
    def run_slow(self):
        return self.testcase_context.getfixturevalue("runslow")

    def __create_testcase_config(self):
        default_config = """[Testdb]
installmode:%s
installpath:%s
syslogngversion:%s
loglevel:%s
runslow:%s""" % (self.syslog_ng_install_mode, self.syslog_ng_install_path, self.syslog_ng_version, self.log_level, self.run_slow)
        with open(self.testdb_path_database.testcase_configuration_file, 'w') as testcase_config_file:
            testcase_config_file.write(default_config)
