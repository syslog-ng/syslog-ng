class IETF(object):
    def __init__(self, testdb_logger):
        self.log_writer = testdb_logger.set_logger("IETF")

    @staticmethod
    def create_ietf_message(generated_message_parts, counter, add_newline=False):
        message = ""
        if "priority" in generated_message_parts:
            message += "<%s>" % generated_message_parts["priority"]
        if "syslog_protocol_version" in generated_message_parts:
            message += "%s " % generated_message_parts["syslog_protocol_version"]
        if "iso_timestamp" in generated_message_parts:
            message += "%s " % generated_message_parts["iso_timestamp"]
        if "hostname" in generated_message_parts:
            message += "%s " % generated_message_parts["hostname"]
        if "program" in generated_message_parts:
            message += "%s " % generated_message_parts["program"]
        if "pid" in generated_message_parts:
            message += "%s " % generated_message_parts["pid"]
        if "message_id" in generated_message_parts:
            message += "%s " % generated_message_parts["message_id"]
        if "sdata" in generated_message_parts:
            message += '[meta sequenceId="%s"] ' % counter
        if "message" in generated_message_parts:
            message += "%s %s" % (generated_message_parts["message"], counter)
        if add_newline:
            message += "\n"

        message_length = len(message.encode('utf-8'))
        message = "%d %s" % (message_length, message)
        return message
