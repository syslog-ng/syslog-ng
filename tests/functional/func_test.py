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

tests = (test_input_drivers, test_sql, test_file_source, test_filters, test_performance)

init_env()



verbose = False
success = True
if len(sys.argv) > 1:
    verbose = True
try:
    for test_module in tests:
        contents = dir(test_module)
        contents.sort()
        for obj in contents:
            if obj[:5] != 'test_':
                continue
            test_case = getattr(test_module, obj)
            test_name = test_module.__name__ + '.' + obj
            print_start(test_name)

            if not start_syslogng(test_module.config, verbose):
                sys.exit(1)

            print_user("Starting test case...")
            if not test_case():
                print_end(test_name, False)
                success = False
                sys.exit(1)

            if not stop_syslogng():
                sys.exit(1)
            if success:
                print_end(test_name, True)
finally:
    stop_syslogng()

if not success:
    sys.exit(1)
