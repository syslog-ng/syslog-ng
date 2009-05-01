#!/usr/bin/env python

import os

#internal modules
from log import *
from messagegen import *
from control import *
from globals import *
import messagegen



def init_env():
    for pipe in ('log-pipe', 'log-padded-pipe'):
        try:
            os.unlink(pipe)
        except OSError:
            pass
        os.mkfifo(pipe)
    try:
        os.mkdir("wildcard")
    except OSError, e:
        if e.errno != errno.EEXIST:
            raise


# import test modules
import test_file_source
import test_filters
import test_input_drivers
import test_performance
import test_sql

tests = (test_file_source, test_filters, test_input_drivers, test_performance, test_sql)

#tests = (
#  (drvtest_conf, (
#     test_input_drivers,
#     test_catchall,
#     test_fallback,
#     test_final,
#     test_indep
#  )),
#  (sql_conf, (
#     test_sql,
#  )),
#  (performance_conf, (
#     test_performance,
#  )),
#  (filter_conf, (
#    test_facility_single,
#    test_facility_multi,
#    test_level_single,
#    test_level_multi)),
#  (wildcard_conf, (
#    test_wildcard_files,)),
#)

init_env()



verbose = False
success = True
if len(sys.argv) > 1:
    verbose = True
try:
    for test_module in tests:
        for obj in dir(test_module):
            if obj[:5] != 'test_':
                continue
            test_case = getattr(test_module, obj)
            test_name = test_module.__name__ + '.' + obj
            print_start(test_name)

            if not start_syslogng(test_module.config, verbose):
                sys.exit(1)
            print_user("Syslog-ng started")

            print_user("Starting test case...")
            if not test_case():
                print_end(test_name, False)
                success = False

            if not stop_syslogng():
                sys.exit(1)
            print_user("syslog-ng stopped")
            print_end(test_name, True)
finally:
    stop_syslogng()

if not success:
    sys.exit(1)
