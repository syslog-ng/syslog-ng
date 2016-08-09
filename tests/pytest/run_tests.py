#!/usr/bin/python3
import sys
import argparse
import pytest
import logging

slng_types = ['ospkg', 'balabitpkg', 'custom']


def get_parser():
    parser = argparse.ArgumentParser()
    parser.add_argument("-f", "--func", action="store_const", const=True, dest="func",
                        help="Run functional tests with pytest, pytest parameters: [-lvs]")
    parser.add_argument("-u", "--unit", action="store_const", const=True, dest="unit",
                        help="Run unit tests with pytest, pytest parameters: [-lvs]")
    parser.add_argument("-t", "--type", dest="type", help="Type of tested syslog_ng available types: %s" % slng_types)
    parser.add_argument("-i", "--install-dir", dest="install_dir",
                        help="Custom install path for syslog_ng, under this directory should be exist: [bin/, etc/, sbin/, var/]")
    parser.add_argument("-v", "--version", dest="version", help="Syslog-ng version: [3.8, 6.0, 7.0]")
    parser.add_argument("-l", "--log-level", dest="log_level", help="Set minimum debug log level: [INFO, DEBUG, ERROR]")
    return parser


def save_testdb_configuration(options):
    configuration_file_path = "testdb_configuration.ini"
    with open(configuration_file_path, mode='w') as configuration_file:
        configuration_file.write("[Testdb]\n")
        configuration_file.write("Type: %s\n" % options.type)
        configuration_file.write("InstallDir: %s\n" % options.install_dir)
        configuration_file.write("Version: %s\n" % options.version)
        configuration_file.write("LogLevel: %s\n" % options.log_level)

def main():
    parser = get_parser()
    opts = parser.parse_args(sys.argv[1:])

    if opts.func:
        test_directory = "func_tests/"
    elif opts.unit:
        test_directory = "src/"
    else:
        logging.error("Missing start parameter, see help with [-h]")
        sys.exit(1)

    if opts.type not in slng_types:
        logging.error("Missing or wrong tested syslog_ng type, see help with [-h]")
        sys.exit(1)
    elif opts.type == "custom" and not opts.install_dir:
        logging.error("Missing custom install path for syslog_ng, see help with [-h]")
        sys.exit(1)

    save_testdb_configuration(opts)

    pytest.main("-lvs %s" % test_directory)

if __name__ == "__main__":
    main()
