import time
import logging
from src.syslog_ng.binary_syslog_ng_ctl.binary_syslog_ng_ctl import SyslogNgCtl

MONITORING_TIME = 5  # 5 sec
POLL_FREQ = 0.001  # 0.001 sec


def wait_for_starting_control_socket():
    control_tool = SyslogNgCtl()
    socket_is_up = False
    stime = atime = time.time()
    while atime <= (stime + MONITORING_TIME):
        if control_tool.is_syslog_ng_running():
            socket_is_up = True
            break
        time.sleep(POLL_FREQ)
        atime = time.time()
    if not socket_is_up:
        logging.warning(
            "The control socket is not created in a short time. socket: %s" % control_tool.get_syslog_ng_control_tool())
        return False
    return True


def wait_for_stopping_control_socket():
    control_tool = SyslogNgCtl()
    socket_is_up = False
    stime = atime = time.time()
    while atime <= (stime + MONITORING_TIME):
        if not control_tool.is_syslog_ng_running():
            socket_is_up = True
            break
        time.sleep(POLL_FREQ)
        atime = time.time()
    if not socket_is_up:
        logging.warning(
            "The control socket is not created in a short time. socket: %s" % control_tool.get_syslog_ng_control_tool())
        return False
    return True


def wait_for_statistics_count(stat):
    logging.info("stat: %s" % stat)
    control_tool = SyslogNgCtl()
    found_stat = False
    stime = atime = time.time()
    while atime <= (stime + MONITORING_TIME):
        if stat in str(control_tool.stats()):
            found_stat = True
            break
        time.sleep(POLL_FREQ)
        atime = time.time()
    if not found_stat:
        logging.info(control_tool.stats())
        logging.warning("The following stat counter not reached: %s" % stat)
        return False
    return True
