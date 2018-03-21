import copy
import socket
from source.message.bsd import BSD
from source.message.ietf import IETF


class MessageInterface(object):
    def __init__(self, testdb_logger, driver_data_provider):
        self.log_writer = testdb_logger.set_logger("MessageInterface")
        self.driver_data_provider = driver_data_provider

        self.default_bsd_message_parts = {
            "priority": "38",
            "bsd_timestamp": "Jun  1 08:05:04",
            "hostname": socket.gethostname(),
            "program": "testprogram",
            "pid": "9999",
            "message": "test \u00c1\u00c9\u0150\u00da\u0170\u00d3\u00dc-\u00e1\u00e1\u00e9\u00fa\u00f3\u00f6 message"
        }
        self.default_ietf_message_parts = {
            "priority": "38",
            "syslog_protocol_version": "1",
            "iso_timestamp": "2017-06-01T08:05:04+02:00",
            "hostname": socket.gethostname(),
            "program": "testprogram",
            "pid": "9999",
            "message_id": "-",
            "sdata": '[meta sequenceId="1"]',
            "message": "test \u00c1\u00c9\u0150\u00da\u0170\u00d3\u00dc-\u00e1\u00e1\u00e9\u00fa\u00f3\u00f6 message"
        }
        self.bsd_syslog = BSD(testdb_logger=testdb_logger)
        self.ietf_syslog = IETF(testdb_logger=testdb_logger)

# Interface for BSDSyslog
    def create_bsd_message(self, defined_bsd_message_parts=None, counter=1, add_newline=False):
        self.validate_defined_message_parts(defined_message_parts=defined_bsd_message_parts, default_message_parts=self.default_bsd_message_parts)
        generated_bsd_message_parts = self.set_message_parts(defined_message_parts=defined_bsd_message_parts, default_message_parts=self.default_bsd_message_parts)
        return self.bsd_syslog.create_bsd_message(generated_message_parts=generated_bsd_message_parts, counter=counter, add_newline=add_newline)

    def create_multiple_bsd_messages(self, defined_bsd_message_parts=None, counter=2, add_newline=False):
        messages = []
        for actual_counter in range(1, counter + 1):
            messages.append(self.create_bsd_message(defined_bsd_message_parts=defined_bsd_message_parts, counter=actual_counter, add_newline=add_newline))
        return messages

# Interface for IETFSyslog
    def create_ietf_message(self, defined_ietf_message_parts=None, counter=1, add_newline=False):
        self.validate_defined_message_parts(defined_message_parts=defined_ietf_message_parts, default_message_parts=self.default_ietf_message_parts)
        generated_ietf_message_parts = self.set_message_parts(defined_message_parts=defined_ietf_message_parts, default_message_parts=self.default_ietf_message_parts)
        return self.ietf_syslog.create_ietf_message(generated_message_parts=generated_ietf_message_parts, counter=counter, add_newline=add_newline)

    def create_multiple_ietf_messages(self, defined_ietf_message_parts=None, counter=2, add_newline=False):
        messages = []
        for actaul_counter in range(1, counter + 1):
            messages.append(self.create_ietf_message(defined_ietf_message_parts=defined_ietf_message_parts, counter=actaul_counter, add_newline=add_newline))
        return messages

# Other
    def create_message_for_source_driver(self, driver_name="file", defined_message_parts=None, counter=1):
        if not defined_message_parts:
            defined_message_parts = {}
        if driver_name in ['file']:
            generated_message = self.create_multiple_bsd_messages(defined_bsd_message_parts=defined_message_parts, counter=counter)
        else:
            self.log_writer.error("Not defined, or unknown driver: %s" % driver_name)
            assert False
        return generated_message

    def create_message_for_destination_driver(self, driver_name="file", defined_message_parts=None, counter=1):
        if not defined_message_parts:
            defined_message_parts = {}
        if driver_name in self.driver_data_provider.get_all_drivers_with_property(property_name="connection_type", property_value="file_based"):
            defined_message_parts['priority'] = "skip"
            generated_message = self.create_multiple_bsd_messages(defined_bsd_message_parts=defined_message_parts, counter=counter, add_newline=True)
        else:
            self.log_writer.error("Not defined, or unknown driver: %s" % driver_name)
            assert False
        return generated_message

    def validate_defined_message_parts(self, defined_message_parts, default_message_parts):
        if defined_message_parts and (set(defined_message_parts) - set(default_message_parts.keys())):
            self.log_writer.error("Found unknown log message part: %s" % defined_message_parts)
            assert False

    @staticmethod
    def set_message_parts(defined_message_parts, default_message_parts):
        generated_message_parts = copy.deepcopy(default_message_parts)
        for message_part in default_message_parts.keys():
            if defined_message_parts:
                if (message_part in defined_message_parts.keys()) and (defined_message_parts[message_part] != "skip"):
                    generated_message_parts[message_part] = defined_message_parts[message_part]
                elif (message_part in defined_message_parts.keys()) and (defined_message_parts[message_part] == "skip"):
                    generated_message_parts.pop(message_part)
        return generated_message_parts
