import logging
import os
import sys


class FileDriver(object):
    def __init__(self):
        self.file_path = None
        self.dir_path = None

    @staticmethod
    def get_last_modification_time(file_path):
        return os.stat(file_path).st_mtime

    @staticmethod
    def get_file_size(file_path):
        return os.stat(file_path).st_size

    @staticmethod
    def is_path_exist(file_path):
        return os.path.exists(file_path)

    def delete_path(self, file_path=None, dir_path=None):
        if file_path:
            path = file_path
        else:
            path = dir_path
        if self.is_path_exist(path):
            try:
                if file_path:
                    os.unlink(file_path)
                    logging.info("File was deleted: [%s]" % file_path)
                if dir_path:
                    os.rmdir(dir_path)
                    logging.info("Dir was deleted: [%s]" % file_path)
                return True
            except OSError as error:
                logging.error("File can not be deleted: [%s], error: [%s]" % (file_path, error))
                sys.exit(1)

    @staticmethod
    def create_dir(directory):
        if not os.path.exists(directory):
            os.makedirs(directory)
            logging.info("Directory created: [%s]" % directory)
            return True
        # else:
        #     logging.info("Directory already exists: [%s], can not create it" % directory)
        #     sys.exit(1)
