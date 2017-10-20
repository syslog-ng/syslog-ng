from source.testdb.common.common import get_current_date, get_testcase_name


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
        pass

    def teardown(self):
        pass
