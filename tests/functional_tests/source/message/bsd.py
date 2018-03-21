class BSD(object):
    def __init__(self, testdb_logger):
        self.log_writer = testdb_logger.set_logger("BSD")

    @staticmethod
    def create_bsd_message(generated_message_parts, counter, add_newline=False):
        message = ""
        if "priority" in generated_message_parts:
            message += "<%s>" % generated_message_parts["priority"]
        if "bsd_timestamp" in generated_message_parts:
            message += "%s " % generated_message_parts["bsd_timestamp"]
        if "hostname" in generated_message_parts:
            message += "%s " % generated_message_parts["hostname"]
        if "program" in generated_message_parts:
            message += "%s" % generated_message_parts["program"]
        if "pid" in generated_message_parts:
            message += "[%s]: " % generated_message_parts["pid"]
        if "message" in generated_message_parts:
            message += "%s %s" % (generated_message_parts["message"], counter)
        if add_newline:
            message += "\n"

        return message
