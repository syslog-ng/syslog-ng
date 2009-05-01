import time

def print_user(msg):
    print '    ', time.strftime('%Y-%M-%dT%H:%m:%S'), msg

def print_start(testcase):
    print("\n\n##############################################")
    print("### Starting testcase: %s" % testcase)
    print("##############################################")
    print_user("Testcase start")

def print_end(testcase, result):
    print_user("Testcase end")
    print("##############################################")
    if result:
        print("### PASS: %s" % testcase)
    else:
        print("### FAIL: %s" % testcase)
    print("##############################################\n\n")

