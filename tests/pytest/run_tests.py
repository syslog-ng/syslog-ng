#!/usr/bin/python3
import sys
import os
import argparse
import pytest
import logging


def get_parser():
    parser = argparse.ArgumentParser()
    parser.add_argument("-r", "--running-test", dest="running_test", default="func", help="Set running test type: [func, unit]; func: run functional tests on installed syslog-ng, unit: run self-test on lib")
    parser.add_argument("-s", "--syslog-ng-install", dest="syslog_ng_install", default="path", help="Set running syslog-ng type: [path, ospkg, balabitpkg, custom]")
    parser.add_argument("-c", "--config-version", dest="config_version", default="3.8", help="Set config version: [3.8]")
    parser.add_argument("-i", "--install-dir", dest="install_dir", help="Custom install path for syslog-ng, under this directory should be exist: [bin/, etc/, sbin/, var/]")
    parser.add_argument("-l", "--log-level", dest="log_level", default="INFO", help="Set minimum debug log level: [INFO, ERROR]")
    return parser


def save_testdb_configuration(options):
    configuration_file_path = os.path.join("/tmp/", "testdb_configuration.ini")
    with open(configuration_file_path, mode='w') as configuration_file:
        configuration_file.write("[Testdb]\n")
        configuration_file.write("RunningTest: %s\n" % options.running_test)
        configuration_file.write("SyslogNgInstall: %s\n" % options.syslog_ng_install)
        configuration_file.write("ConfigVersion: %s\n" % options.config_version)
        configuration_file.write("InstallDir: %s\n" % options.install_dir)
        configuration_file.write("LogLevel: %s\n" % options.log_level)


def main():
    parser = get_parser()
    opts = parser.parse_args(sys.argv[1:])

    test_directory = "func_tests/"
    if opts.running_test == "unit":
        test_directory = "src/"

    if opts.syslog_ng_install == "custom" and not opts.install_dir:
        logging.error("Missing custom install path for syslog_ng, see help with [-h]")
        sys.exit(1)

    save_testdb_configuration(opts)
    pytest.main(test_directory)

if __name__ == "__main__":
    main()
