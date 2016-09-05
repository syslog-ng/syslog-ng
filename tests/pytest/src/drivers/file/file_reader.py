import logging

from src.common.waiters import wait_till_before_after_not_equals
from src.common.waiters import wait_till_statement_not_true
from src.common.waiters import wait_till_statement_not_zero
from src.drivers.file.file_driver import FileDriver


class FileReader(FileDriver):
    def __init__(self):
        FileDriver.__init__(self)

    def count_lines(self, file_path):
        self.file_path = file_path
        file_content = self.get_messages(self.file_path)
        return file_content.count("\n")

    def dump_file_content(self, file_path):
        file_content = self.get_messages(file_path)
        logging.info("File path: [%s]" % file_path)
        logging.info("File content: [%s]" % file_content)

    def get_messages(self, file_path):
        if self.wait_till_file_not_created(file_path):
            if self.wait_till_file_not_change(file_path):
                with open(file_path, 'r') as file_object:
                    file_content = file_object.read()
                return file_content

    def wait_till_file_not_change(self, file_path):
        return wait_till_before_after_not_equals(self.get_last_modification_time, file_path)

    def wait_till_file_not_created(self, file_path):
        return wait_till_statement_not_true(self.is_path_exist, file_path)

    def wait_for_expected_message(self, file_path, expected_message):
        file_content = self.get_messages(file_path)
        if file_content:
            return expected_message in file_content
        else:
            return False

    def wait_till_file_size_not_zero(self, file_path):
        return wait_till_statement_not_zero(self.get_file_size, file_path)
