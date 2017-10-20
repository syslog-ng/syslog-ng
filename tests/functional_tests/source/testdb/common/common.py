import binascii
import os
import time
from datetime import datetime

UNIQUE_LENGTH = 5
MONITORING_TIME = 10
poll_freq = 0.001


def get_current_date():
    return datetime.now().strftime('%Y-%m-%d-%H-%M-%S-%f')


def get_unique_id(length=UNIQUE_LENGTH):
    return binascii.b2a_hex(os.urandom(16)).decode('ascii')[0:length]


def get_testcase_name(testcase_context):
    if testcase_context.node.originalname:
        return testcase_context.node.originalname
    elif testcase_context.node.name:
        return testcase_context.node.name


def wait_till_before_after_not_equals(func, args=None, monitoring_time=MONITORING_TIME):
    equal_counter = 0
    expected_equal_counter = 100
    t_end = time.monotonic() + monitoring_time
    while time.monotonic() <= t_end:
        if args:
            result_before = func(args)
            time.sleep(poll_freq)
            result_after = func(args)
        else:
            result_before = func()
            time.sleep(poll_freq)
            result_after = func()
        if result_before == result_after:
            equal_counter += 1
        if equal_counter == expected_equal_counter:
            return True
    return False


def wait_till_function_not_true(func, arg1=None, arg2=None, monitoring_time=MONITORING_TIME):
    t_end = time.monotonic() + monitoring_time
    while time.monotonic() <= t_end:
        if arg1 and not arg2:
            if func(arg1):
                return True
        if arg1 and arg2:
            if func(arg1, arg2):
                return True
        if not arg1 and not arg2:
            if func():
                return True
        time.sleep(poll_freq)
    return False


def wait_till_function_not_false(func, arg1=None, arg2=None, monitoring_time=MONITORING_TIME):
    t_end = time.monotonic() + monitoring_time
    while time.monotonic() <= t_end:
        if arg1 and not arg2:
            if not func(arg1):
                return True
        if arg1 and arg2:
            if not func(arg1, arg2):
                return True
        if not arg1 and not arg2:
            if not func():
                return True
        time.sleep(poll_freq)
    return False
