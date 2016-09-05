class MessageGenerator(object):
    def __init__(self):
        pass

    def input_message(self, driver_from_to, counter_from_to=None, counter=None, custom_message_parts=None):
        # Note: We should know the driver_from, we can gen. bsd or syslog message too
        # driver_from_to = ["file", "file"]
        # counter_from_to = [1, 100]
        # counter = 5
        # custom_message_parts = {"host": "mycomputer", "program": "myprogram"}
        pass

    def expected_output_message(self):
        pass

    def format_raw_message(self):
        pass

    def bsd_message(self, driver_from_to=None, counter_from_to=None, counter=None, custom_message_parts=None):
        return "<34>Oct 11 22:14:15 mymachine su: 'su root' failed for lonvick on /dev/pts/8"

    def syslog_message(self, driver_from_to=None, counter_from_to=None, counter=None, custom_message_parts=None):
        pass

