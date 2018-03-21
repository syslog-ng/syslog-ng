import os


class TestdbPathDatabase(object):
    def __init__(self, testcase_name, topology, current_date):
        self.testcase_name = testcase_name
        self.topology = topology
        self.current_date = current_date

        self.__create_testcase_working_dir()

    @property
    def testdb_root_dir(self):
        cwd = os.getcwd()
        if cwd.endswith("source"):
            return os.path.abspath(os.path.join(cwd, os.pardir))
        return cwd

    @property
    def testdb_report_dir(self):
        return os.path.join(self.testdb_root_dir, 'reports')

    @property
    def testcase_working_dir(self):
        return os.path.join(self.testdb_report_dir, self.current_date)

    @property
    def testcase_report_file(self):
        return os.path.join(self.testcase_working_dir, "testcase_%s_%s.log" % (self.testcase_name, self.topology))

    @property
    def testcase_configuration_file(self):
        return os.path.join(self.testcase_working_dir, "testcase_configuration.ini")

    @property
    def libjvm_dir(self):
        return "/usr/lib/jvm/default-java/jre/lib/amd64/server/"

    def __create_testcase_working_dir(self):
        if not os.path.exists(self.testcase_working_dir):
            os.makedirs(self.testcase_working_dir)
        with open(self.testcase_report_file, 'w') as file_object:
            file_object.write("First line\n")
