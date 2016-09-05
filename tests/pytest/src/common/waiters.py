import time

monitoring_time = 2000
poll_freq = 0.001

def wait_till_before_after_not_equals(function, args=None):
    equal_counter = 0
    expected_equal_counter = 100
    for counter in range(1, 2000):
        if args:
            result_before = function(args)
            time.sleep(poll_freq)
            result_after = function(args)
        else:
            result_before = function()
            time.sleep(poll_freq)
            result_after = function()
        if result_before == result_after:
            equal_counter += 1
        if equal_counter == expected_equal_counter:
            return True
    return False


def wait_till_statement_not_true(function, args=None):
    for counter in range(1, 2000):
        if args:
            if function(args):
                return True
        else:
            if function():
                return True
        time.sleep(poll_freq)
    return False


def wait_till_statement_not_false(function, args=None):
    for counter in range(1, 2000):
        if args:
            if not function(args):
                return True
        else:
            if not function():
                return True
        time.sleep(poll_freq)
    return False


def wait_till_statement_not_zero(function, args):
    pass
