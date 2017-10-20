from source.testdb.common.common import get_current_date, get_testcase_name
from source.testdb.path.path_database import TestdbPathDatabase


class SetupClasses(object):
    def __init__(self, testcase_context, topology="server"):
        self.testcase_context = testcase_context
        self.topology = topology

        self.current_date = get_current_date()
        self.testcase_name = get_testcase_name(self.testcase_context)
        self.log_writer = None

        self.instantiate_classes()
        self.testcase_context.addfinalizer(self.teardown)

    def instantiate_classes(self):
        if self.topology == "server":
            self.init_classes_for_topology(testcase_context=self.testcase_context, topology="server")
        elif self.topology == "client_server":
            self.init_classes_for_topology(testcase_context=self.testcase_context, topology="server")
            self.init_classes_for_topology(testcase_context=self.testcase_context, topology="client")
        else:
            print("Unknown defined topology: [%s]" % self.topology)
            assert False

    def init_classes_for_topology(self, testcase_context, topology):
        setattr(self, "testdb_path_database_for_%s" % topology,
                TestdbPathDatabase(
                    testcase_name=self.testcase_name,
                    topology=topology,
                    current_date=self.current_date
                ))

        if topology == "server":
            self.testdb_path_database = self.testdb_path_database_for_server


    def teardown(self):
        pass
