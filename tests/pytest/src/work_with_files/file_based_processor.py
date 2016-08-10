import time
import os
import stat
import logging
from src.syslog_ng.path_handler.path_handler import SyslogNgPathHandler


class FileBasedProcessor(object):
    def __init__(self):
        self.monitoring_time = 2  # 1 sec
        self.poll_freq = 0.001  # 0.001 sec
        # self.path_handler = SyslogNgPathHandler()
        self.global_config = None
        self.written_files = []

    def set_global_config(self, global_config):
        self.global_config = global_config

    def create_file_with_content(self, file_path, content):
        self.written_files.append(file_path)
        logging.info("Write file: [%s] with content: [%s]" % (file_path, content))
        with open(file_path, "w") as file_object:
            file_object.write(content)
            file_object.flush()

    def delete_file(self, path):
        logging.info("Deleting file: [%s]" % path)
        if os.path.exists(path):
            try:
                os.remove(path)
            except OSError as error:
                logging.error("File can not be deleted: [%s], error: [%s]" % (path, error))


    def delete_written_files(self):
        for written_file in self.written_files:
            self.delete_file(path=written_file)

    def write_message_to_file(self, file_path, input_message, driver_name=None):
        file_path = file_path.replace('"', '')
        self.written_files.append(file_path)
        if driver_name == "program":
            try:
                with open(file_path, "a") as file_object:
                    file_object.write("#!/bin/bash\necho '%s'\n" % input_message)
                    file_object.flush()
                file_object.close()
                os.chmod(file_path, stat.S_IRWXU | stat.S_IRGRP | stat.S_IROTH)
            except IOError as error:
                pass
        else:
            if "@version" in input_message:
                with open(file_path, 'w') as file_object:
                    file_object.write("%s\n" % input_message)
                    file_object.flush()
            else:
                with open(file_path, 'a') as file_object:
                    file_object.write("%s\n" % input_message)
                    file_object.flush()
        logging.info("Message written to file. Message: [%s], File: [%s]" % (input_message, file_path))

    def count_lines_in_file(self, file_path):
        pass

    def dump_file_content(self, file_path, mode=None):
        file_path = file_path.replace('"', '')
        logging.warning("Dumped file content: %s" % file_path)
        with open(file_path) as file_object:
            for line in file_object.readlines():
                print(line.strip())

    def get_messages_from_file(self, file_path):
        file_path = file_path.replace('"', '')
        with open(file_path) as file_object:
            file_content = file_object.read().splitlines()
        logging.info("Lines are readed from file. Lines: [%s], File: [%s]" % (file_content, file_path))
        return file_content

    def dump_actual_information(self, file_path):
        file_path = file_path.replace('"', '')
        self.global_config['testrun_reporter'].dump_testrun_information_on_failed()
        print("\n########### Print Testrun Information: ###########")
        print(self.written_files)
        print("\n########### syslog-ng console log: ###########")
        self.dump_file_content(self.path_handler.get_syslog_ng_console_log(), "raw")
        # print("############ DUMP Statistics ##############")
        # print(self.slng_controll_tool.get_stats())
        # print("############ DUMP Logpath info ##############")
        # self.dump_file_content(self.path_handler.get_syslog_ng_console_log(), "raw")
        # print("############ DUMP Logpath info ##############")
        # self.group_data_provider.dump_logpath_info_for_missed_file_path(file_path)
        # internal_file_path = self.group_data_provider.get_destination_group_data_for_connected_source("internal", "file_path")
        print("############ DUMP Internal log: %s ##############" % self.global_config['config'].get_internal_log_output_path())
        self.dump_file_content(self.global_config['config'].get_internal_log_output_path(), "raw")
        # print("############ DUMP File content: %s ##############" % file_path)
        # self.dump_file_content(file_path)

    ### Wait_for functions
    def wait_for_file_creation(self, file_path):
        file_path = file_path.replace('"', '')
        stime = atime = time.time()
        while atime <= (stime + self.monitoring_time):
            if os.path.isfile(file_path):
                logging.info("File has been created. File: [%s]" % file_path)
                return True
            time.sleep(self.poll_freq)
            atime = time.time()
        self.dump_actual_information(file_path)
        raise SystemExit(1)

    def wait_for_expected_message_in_file(self, file_path, expected_message):
        file_path = file_path.replace('"', '')
        message_is_arrived = False
        stime = atime = time.time()
        while atime <= (stime + self.monitoring_time):
            with open(file_path) as file_object:
                for line in file_object:
                    if expected_message in line:
                        message_is_arrived = True
                        break
                if message_is_arrived:
                    logging.info("Expected message exist in file. Message: [%s], File: [%s]" % (expected_message, file_path))
                    return True
                time.sleep(self.poll_freq)
                atime = time.time()
        self.dump_actual_information(file_path)
        raise SystemExit(1)

    def wait_for_file_not_change(self, file_path):
        file_path = file_path.replace('"', '')
        poll_counter = 0
        poll_max_counter = 100
        stime = atime = time.time()
        while atime <= (stime + self.monitoring_time):
            file_mtime_before = os.stat(file_path).st_mtime
            time.sleep(self.poll_freq)
            file_mtime_after = os.stat(file_path).st_mtime
            if file_mtime_before == file_mtime_after:
                poll_counter += 1
            if poll_counter == poll_max_counter:
                ### is the return ok?
                return True
            atime = time.time()
        self.dump_actual_information(file_path)
        raise SystemExit(1)

# def delete_list_of_files(file_list):
#     for file_name in file_list:
#         if os.path.exists(file_name):
#             try:
#                 os.remove(file_name)
#             except OSError as error:
#                 debug_logging.print_error_message(error_message="OSError occurred: %s" % error)
