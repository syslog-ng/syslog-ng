import socket


class MessageGenerator(object):
    def __init__(self, customized_message_parts=None, counter=1):
        self.customized_message_parts = customized_message_parts
        self.counter = counter
        self.driver_category_1 = ["file", "tcp", "tcp6", "udp", "program", "network", "udp6"]
        self.driver_category_2 = ["pipe", "unix_dgram", "unix_stream"]
        self.driver_category_3 = ["syslog"]

        self.bsd_message_parts = self.create_bsd_message_parts()
        self.syslog_message_parts = self.create_syslog_message_parts()

    def create_bsd_message_parts(self):
        bsd_message_parts = {
            "priority": self.get_priority_field(),
            "bsd_timestamp": self.get_bsd_timestamp_field(),
            "hostname": self.get_hostname_field(),
            "program": self.get_program_field(),
            "pid": self.get_pid_field(),
            "message": self.get_message_field(),
        }
        return bsd_message_parts

    def create_syslog_message_parts(self):
        syslog_message_parts = {
            "priority": self.get_priority_field(),
            "syslog_protocol_version": "1",
            "iso_timestamp": self.get_iso_timestamp_field(),
            "hostname": self.get_hostname_field(),
            "program": self.get_program_field(),
            "pid": self.get_pid_field(),
            "message_id": "-",
            "sdata": self.get_sdata_field(),
            "message": self.get_message_field(),
        }
        return syslog_message_parts

    def get_priority_field(self):
        if "priority" in self.customized_message_parts.keys():
            return self.customized_message_parts['priority']
        else:
            return "13"

    def get_bsd_timestamp_field(self):
        if "bsd_timestamp" in self.customized_message_parts.keys():
            return self.customized_message_parts['bsd_timestamp']
        else:
            return "Oct 11 22:14:15"

    def get_iso_timestamp_field(self):
        if "iso_timestamp" in self.customized_message_parts.keys():
            return self.customized_message_parts['iso_timestamp']
        else:
            return "2003-10-11T22:14:15.003Z"

    def get_hostname_field(self):
        if "hostname" in self.customized_message_parts.keys():
            if "self" in self.customized_message_parts['hostname']:
                return self.customized_message_parts['hostname'].replace("self", socket.gethostname())
            else:
                return self.customized_message_parts['hostname']
        else:
            return socket.gethostname()

    def get_program_field(self):
        if "program" in self.customized_message_parts.keys():
            return self.customized_message_parts['program']
        else:
            return "test-program"

    def get_pid_field(self):
        if "pid" in self.customized_message_parts.keys():
            return self.customized_message_parts['pid']
        else:
            return "9999"

    def get_sdata_field(self):
        if "sdata" in self.customized_message_parts.keys():
            return self.customized_message_parts['sdata']
        else:
            return "-"

    def get_message_field(self):
        if "message" in self.customized_message_parts.keys():
            return self.customized_message_parts['message']
        else:
            return "test message"

    def format_message_part(self, source_driver, destination_driver, counter):
        return "%s - source_driver: %s - destination_driver: %s - counter: %s" % (
        self.bsd_message_parts['message'], source_driver, destination_driver, counter)

    def generate_input_messages(self, source_driver_name, destination_driver_name):
        return self.generate_messages(source_driver_name, destination_driver_name, "source")

    def generate_output_messages(self, source_driver_name, destination_driver_name):
        return self.generate_messages(source_driver_name, destination_driver_name, "destination")

    def generate_messages(self, source_driver, destination_driver, message_side):
        messages = []
        for i_counter in range(1, self.counter + 1):
            formatted_message = self.format_message_part(source_driver, destination_driver, i_counter)
            if message_side == "source" and source_driver in self.driver_category_1:
                message = self.create_bsd_message(formatted_message)
            elif message_side == "source" and source_driver in self.driver_category_2:
                message = self.create_bsd_message_without_hostname(formatted_message)
            elif message_side == "source" and source_driver in self.driver_category_3:
                message = self.create_syslog_message(formatted_message)

            elif message_side == "destination" and destination_driver in self.driver_category_1:
                if source_driver == "syslog":
                    message = self.create_bsd_message_without_priority_with_pid(formatted_message)
                else:
                    message = self.create_bsd_message_without_priority(formatted_message)
            elif message_side == "destination" and destination_driver in self.driver_category_2:
                message = self.create_bsd_message_without_priority_and_with_self_hostname(formatted_message)
            elif message_side == "destination" and destination_driver in self.driver_category_3:
                message = self.create_syslog_message(formatted_message)
            else:
                assert False

            messages.append(message)
        return messages

    def create_bsd_message(self, formatted_message):
        return "<%s>%s %s %s: %s" % (
            self.bsd_message_parts['priority'],
            self.bsd_message_parts['bsd_timestamp'],
            self.bsd_message_parts['hostname'],
            self.bsd_message_parts['program'],
            formatted_message)

    def create_bsd_message_without_hostname(self, formatted_message):
        return "<%s>%s %s: %s" % (
            self.bsd_message_parts['priority'],
            self.bsd_message_parts['bsd_timestamp'],
            self.bsd_message_parts['program'],
            formatted_message)

    def create_bsd_message_without_priority(self, formatted_message):
        return "%s %s %s: %s" % (
            self.bsd_message_parts['bsd_timestamp'],
            self.bsd_message_parts['hostname'],
            self.bsd_message_parts['program'],
            formatted_message)

    def create_bsd_message_without_priority_with_pid(self, formatted_message):
        return "%s %s %s[%s]: %s" % (
            self.bsd_message_parts['bsd_timestamp'],
            self.bsd_message_parts['hostname'],
            self.bsd_message_parts['program'],
            self.bsd_message_parts['pid'],
            formatted_message)

    def create_bsd_message_without_priority_and_with_self_hostname(self, formatted_message):
        return "%s %s %s: %s" % (
            self.bsd_message_parts['bsd_timestamp'],
            socket.gethostname(),
            self.bsd_message_parts['program'],
            formatted_message)

    def create_syslog_message(self, formatted_message):
        message = "<%s>%s %s %s %s %s %s %s %s" % (
            self.syslog_message_parts['priority'],
            self.syslog_message_parts['syslog_protocol_version'],
            self.syslog_message_parts['iso_timestamp'],
            self.syslog_message_parts['hostname'],
            self.syslog_message_parts['program'],
            self.syslog_message_parts['pid'],
            self.syslog_message_parts['message_id'],
            self.syslog_message_parts['sdata'],
            formatted_message)
        message_with_length = self.add_message_length_before_the_message(message)
        return message_with_length

    def add_message_length_before_the_message(self, message):
        message_length = len(message)
        return "%d %s" % (message_length, message)
