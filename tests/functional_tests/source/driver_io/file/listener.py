class FileListener(object):
    def __init__(self, testdb_logger, file_common):
        self.testdb_logger = testdb_logger
        self.log_writer = testdb_logger.set_logger("FileListener")
        self.file_common = file_common

    def wait_for_content(self, file_path, driver_name, raw=False, message_counter=1):
        if not self.file_common.is_file_exist(file_path):
            self.file_common.wait_for_file_creation(file_path)
        self.file_common.wait_for_expected_number_of_lines_in_file(file_path=file_path, expected_lines=message_counter)
        if driver_name == "file":
            return self.get_content_from_regular_file(file_path, raw=raw)
        else:
            self.log_writer.error("Unknown driver: %s" % driver_name)
            assert False

    def get_content_from_regular_file(self, file_path, raw=True):
        with open(file_path, 'r') as file_object:
            file_content = file_object.read()
        self.log_writer.info("SUBSTEP Content received\n>>>Content: [%s] \n>>>From file: [%s]" % (file_content, file_path))
        if raw:
            return file_content
        return file_content.splitlines(keepends=True)

    def dump_file_content(self, file_path, message_level="info"):
        file_content = self.get_content_from_regular_file(file_path, raw=True)
        if message_level == "info":
            self.log_writer.info("Dump file content, file_path: [%s], content: \n[%s]" % (file_path, file_content))
        elif message_level == "error":
            self.log_writer.error("Dump file content, file_path: [%s], content: \n[%s]" % (file_path, file_content))
