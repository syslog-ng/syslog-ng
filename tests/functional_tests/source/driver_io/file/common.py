import os
import shutil
import stat
from itertools import takewhile, repeat
from source.testdb.common.common import wait_till_function_not_true, wait_till_before_after_not_equals


class FileCommon(object):
    def __init__(self, testdb_logger):
        self.log_writer = testdb_logger.set_logger("FileCommon")
        self.testdb_logger = testdb_logger

    @staticmethod
    def is_file_exist(file_path):
        result_file_exist = os.path.exists(file_path)
        return result_file_exist

    @staticmethod
    def is_dir_exist(dir_path):
        result_dir_exist = os.path.isdir(dir_path)
        return result_dir_exist

    def is_regular_file(self, file_path):
        if self.is_file_exist(file_path):
            return stat.S_ISREG(os.stat(file_path).st_mode)
        return False

    def is_named_pipe(self, file_path):
        if self.is_file_exist(file_path):
            return stat.S_ISFIFO(os.stat(file_path).st_mode)
        return False

    @staticmethod
    def copy_file(source_path, destination_path):
        shutil.copyfile(source_path, destination_path)

    @staticmethod
    def move_file(source_path, destination_path):
        shutil.move(source_path, destination_path)

    def delete_file(self, file_path):
        if self.is_file_exist(file_path):
            os.unlink(file_path)

    def delete_dir(self, dir_path):
        if self.is_dir_exist(dir_path):
            os.rmdir(dir_path)

    def delete_files_from_dir_with_extension(self, subdir, extension):
        files_in_subdir = os.listdir(subdir)
        for file_path in files_in_subdir:
            if file_path.endswith(extension):
                self.delete_file(os.path.join(subdir, file_path))

    def count_lines_in_file(self, file_path):
        if self.is_file_exist(file_path):
            with open(file_path, 'rb') as file_object:
                bufgen = takewhile(lambda x: x, (file_object.read(1024 * 1024) for _ in repeat(None)))
                return sum(buf.count(b'\n') for buf in bufgen)

    def is_expected_number_of_lines_in_file(self, file_path, expected_number_of_lines):
        current_line = self.count_lines_in_file(file_path=file_path)
        return current_line == expected_number_of_lines

    def create_dir(self, dir_path):
        if not self.is_dir_exist(dir_path):
            os.makedirs(dir_path)

    def get_file_size(self, file_path):
        if self.is_file_exist(file_path):
            return os.stat(file_path).st_size

    def get_last_modification_time(self, file_path):
        if self.is_file_exist(file_path):
            return os.stat(file_path).st_mtime

    @staticmethod
    def is_message_in_file(file_path, expected_message):
        file_fd = open(file_path)

        def read8k():
            return file_fd.read(8192)

        for chunk in iter(read8k, ''):
            if expected_message in chunk:
                return True
        return False

    @staticmethod
    def count_message_occurance_in_file(file_path, expected_message):
        file_fd = open(file_path)
        found_occurance = 0

        def read_entire_file():
            return file_fd.read()

        for chunk in iter(read_entire_file, ''):
            found_occurance += chunk.count(expected_message)
        return found_occurance

    @staticmethod
    def grep_in_subdir(pattern, subdir="functional_tests/", extension=".py"):
        list_of_files = []
        for root, dirs, files in os.walk(subdir):
            for file_path in files:
                if (file_path.endswith(extension)) and ("init" not in file_path):
                    actual_file = os.path.join(root, file_path)
                    with open(actual_file, "r") as file_object:
                        if pattern in file_object.read(8192):
                            list_of_files.append(actual_file)
        return sorted(list_of_files)

    def save_testcase_file(self, testcase_name, destination_path):
        self.log_writer.info("STEP Saving testcase file")
        testcase_file = self.grep_in_subdir(pattern=testcase_name)
        assert len(testcase_file) == 1
        testcase_file_base_name = os.path.basename(testcase_file[0])
        destination_path_with_filename = os.path.join(destination_path, testcase_file_base_name)
        self.copy_file(source_path=testcase_file[0], destination_path=destination_path_with_filename)

    def wait_for_messages_in_file(self, file_path, expected_message, expected_occurance=1):
        result_file_creation = self.wait_for_file_creation(file_path)
        result_file_change = self.wait_for_file_not_change(file_path)
        message_occurance = self.count_message_occurance_in_file(file_path, expected_message)
        self.log_writer.info("Actual message occurance: %s" % message_occurance)
        self.log_writer.info("Expected message occurance: %s" % expected_occurance)
        if message_occurance != expected_occurance:
            self.log_writer.error("Expected occurance not equals with actual message occurance.\nexpected message:[%s]\nexpected occurance:[%s]\nactual occurance:[%s]" % (expected_message, expected_occurance, message_occurance))
        return result_file_creation and result_file_change and (message_occurance == expected_occurance)

    def wait_for_one_line_in_file(self, file_path, expected_line=1):
        result_file_creation = self.wait_for_file_creation(file_path)
        result_file_change = self.wait_for_file_not_change(file_path)
        lines_in_file = self.count_lines_in_file(file_path)
        return result_file_creation and result_file_change and (lines_in_file == expected_line)

    def wait_for_expected_number_of_lines_in_file(self, file_path, expected_lines=1):
        result_expected_lines_arrived = wait_till_function_not_true(self.is_expected_number_of_lines_in_file, file_path, expected_lines)
        return result_expected_lines_arrived

    def wait_for_file_creation(self, file_path, monitoring_time=2):
        result_file_creation = wait_till_function_not_true(self.is_file_exist, file_path, monitoring_time=monitoring_time)
        self.testdb_logger.write_message_based_on_value(logsource=self.log_writer, message="File created, file_path: [%s]" % file_path, value=result_file_creation)
        return result_file_creation

    def wait_for_dir_creation(self, dir_path, monitoring_time=2):
        result_dir_creation = wait_till_function_not_true(self.is_dir_exist, dir_path, monitoring_time=monitoring_time)
        self.testdb_logger.write_message_based_on_value(logsource=self.log_writer, message="Dir created, dir_path: [%s]" % dir_path, value=result_dir_creation)
        return result_dir_creation

    def wait_for_file_not_change(self, file_path, monitoring_time=5):
        result_file_change = wait_till_before_after_not_equals(self.get_last_modification_time, file_path, monitoring_time=monitoring_time)
        self.testdb_logger.write_message_based_on_value(logsource=self.log_writer, message="File stop changing, file_path: [%s]" % file_path, value=result_file_change)
        return result_file_change
