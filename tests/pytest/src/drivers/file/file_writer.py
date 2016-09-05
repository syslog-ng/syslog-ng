import logging
import sys

from src.drivers.file.file_driver import FileDriver


class FileWriter(FileDriver):
    def __init__(self):
        FileDriver.__init__(self)

    def create_file_with_content(self, file_path, content):
        if not self.is_path_exist(file_path):
            with open(file_path, "w") as file_object:
                file_object.write("%s\n" % content)
        else:
            logging.error("File [%s] already exist" % file_path)
            sys.exit(1)

    @staticmethod
    def append_file_with_content(file_path, content):
        with open(file_path, "a") as file_object:
            file_object.write("%s\n" % content)

    def rewrite_file_with_content(self, file_path, content):
        if self.is_path_exist(file_path):
            with open(file_path, "w") as file_object:
                file_object.write("%s\n" % content)
        else:
            logging.error("File [%s] not yet exist" % file_path)
            sys.exit(1)
