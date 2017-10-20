from source.testdb.common.common import get_current_date, get_testcase_name
from source.testdb.path.path_database import TestdbPathDatabase
from source.testdb.config.config_context import TestdbConfigContext
from source.testdb.logger.logger import TestdbLogger
from source.register.file import FileRegister
from source.driver_io.file.common import FileCommon
from source.driver_io.file.writer import FileWriter
from source.driver_io.file.listener import FileListener
from source.syslog_ng.drivers.driver_data_provider import DriverDataProvider
from source.syslog_ng.configuration.interface import SyslogNgConfigInterface
from source.message.interface import MessageInterface
from source.executor.executor_interface import ExecutorInterface


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
        setattr(self, "testdb_config_context_for_%s" % topology,
                TestdbConfigContext(
                    testdb_path_database=getattr(self, "testdb_path_database_for_%s" % topology),
                    testcase_context=testcase_context
                ))
        setattr(self, "testdb_logger_for_%s" % topology,
                TestdbLogger(
                    testdb_path_database=getattr(self, "testdb_path_database_for_%s" % topology),
                    testdb_config_context=getattr(self, "testdb_config_context_for_%s" % topology)
                ))

        self.log_writer = self.testdb_logger_for_server.set_logger(logsource="SetupClasses")
        self.log_writer.info("=============================Testcase start: [%s]================================" % self.testcase_name)
        self.log_writer.info(">>> Testcase log file: %s", getattr(self, "testdb_path_database_for_%s" % topology).testcase_report_file)
        self.log_writer.info(">>> Testcase log level: %s", getattr(self, "testdb_config_context_for_%s" % topology).log_level)

        setattr(self, "file_register_for_%s" % topology,
                FileRegister(
                    testdb_logger=getattr(self, "testdb_logger_for_%s" % topology),
                    testdb_path_database=getattr(self, "testdb_path_database_for_%s" % topology),
                ))
        setattr(self, "file_common_for_%s" % topology,
                FileCommon(
                    testdb_logger=getattr(self, "testdb_logger_for_%s" % topology),
                ))
        setattr(self, "file_writer_for_%s" % topology,
                FileWriter(
                    testdb_logger=getattr(self, "testdb_logger_for_%s" % topology),
                    file_common=getattr(self, "file_common_for_%s" % topology)
                ))
        setattr(self, "file_listener_for_%s" % topology,
                FileListener(
                    testdb_logger=getattr(self, "testdb_logger_for_%s" % topology),
                    file_common=getattr(self, "file_common_for_%s" % topology)
                ))

        writers = {
            "file": getattr(self, "file_writer_for_%s" % topology),
        }

        listeners = {
            "file": getattr(self, "file_listener_for_%s" % topology),
        }

        registers = {
            "file": getattr(self, "file_register_for_%s" % topology),
        }

        setattr(self, "driver_data_provider_for_%s" % topology,
                DriverDataProvider(
                    testdb_logger=getattr(self, "testdb_logger_for_%s" % topology),
                    writers=writers,
                    listeners=listeners,
                    registers=registers
                ))
        setattr(self, "syslog_ng_config_interface_for_%s" % topology,
                SyslogNgConfigInterface(
                    testdb_logger=getattr(self, "testdb_logger_for_%s" % topology),
                    testdb_config_context=getattr(self, "testdb_config_context_for_%s" % topology),
                    testdb_path_database=getattr(self, "testdb_path_database_for_%s" % topology),
                    driver_data_provider=getattr(self, "driver_data_provider_for_%s" % topology),
                    file_register=getattr(self, "file_register_for_%s" % topology),
                    file_writer=getattr(self, "file_writer_for_%s" % topology),
                    topology=topology
                ))
        setattr(self, "message_interface_for_%s" % topology,
                MessageInterface(
                    testdb_logger=getattr(self, "testdb_logger_for_%s" % topology),
                    driver_data_provider=getattr(self, "driver_data_provider_for_%s" % topology),
                ))
        setattr(self, "executor_interface_for_%s" % topology,
                ExecutorInterface(
                    testdb_logger=getattr(self, "testdb_logger_for_%s" % topology),
                    testdb_path_database=getattr(self, "testdb_path_database_for_%s" % topology),
                ))

        if topology == "server":
            self.testdb_path_database = self.testdb_path_database_for_server
            self.testdb_config_context = self.testdb_config_context_for_server
            self.testdb_logger = self.testdb_logger_for_server

            self.file_register = self.file_register_for_server
            self.file_common = self.file_common_for_server
            self.file_writer = self.file_writer_for_server
            self.file_listener = self.file_listener_for_server

            self.driver_data_provider = self.driver_data_provider_for_server
            self.syslog_ng_config_interface = self.syslog_ng_config_interface_for_server
            self.message_interface = self.message_interface_for_server
            self.executor_interface = self.executor_interface_for_server

    def teardown(self):
        self.log_writer.info("=============================Testcase finish: [%s]================================" % self.testcase_name)
        self.log_writer.info(">>> Testcase log file: %s" % self.testdb_path_database.testcase_report_file)

