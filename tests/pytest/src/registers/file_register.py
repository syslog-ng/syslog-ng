import os
import uuid
from datetime import datetime

from src.common.singleton import Singleton
from src.drivers.file.file_driver import FileDriver
from src.registers.global_register import GlobalRegister


@Singleton
class FileRegister(GlobalRegister):
    def __init__(self):
        GlobalRegister.__init__(self)
        self.registered_filenames = {}
        self.global_rootdir = "/tmp"
        self.current_date = datetime.now().strftime("%Y-%m-%d-%H-%M-%S-%f")
        self.testcase_subdir = os.path.join(self.global_rootdir, self.current_date)
        self.filedriver = FileDriver()

    def __del__(self):
        pass

    def get_registered_filename(self, prefix, extension="txt", subdir=None):
        if self.is_item_registered(key=prefix, group=self.registered_filenames):
            return self.get_registered_item(key=prefix, group=self.registered_filenames)
        if subdir:
            subdir = os.path.join(self.testcase_subdir, subdir)
        else:
            subdir = self.testcase_subdir

        self.filedriver.create_dir(directory=subdir)
        unique_filename = self.create_and_register_unique_filename(prefix=prefix, subdir=subdir, extension=extension)
        return unique_filename

    def delete_registered_files(self):
        for registered_file in self.registered_filenames:
            self.filedriver.delete_path(file_path=self.registered_filenames[registered_file])
        self.registered_filenames = {}

    def create_and_register_unique_filename(self, prefix, subdir=None, extension="txt"):
        unique_filename = self.create_unique_filename(subdir=subdir, prefix=prefix, extension=extension)
        self.register_item_in_group(key=prefix, value=unique_filename, group=self.registered_filenames)
        return unique_filename

    @staticmethod
    def create_unique_filename(subdir, prefix, extension):
        while True:
            filename_pattern = "%s_%s.%s" % (prefix, str(uuid.uuid4()), extension)
            unique_filename = os.path.join(subdir, filename_pattern)
            return unique_filename
