import time
import logging

MONITORING_TIME = 5  # 5 sec
POLL_FREQ = 0.001  # 0.001 sec

class SyslogNgCtlUtils(object):
    def __init__(self):
        self.global_config = None

    def set_global_config(self, global_config):
        self.global_config = global_config
        self.control_tool = self.global_config['syslog_ng_ctl']

    def wait_for_starting_control_socket(self):
        socket_is_up = False
        stime = atime = time.time()
        while atime <= (stime + MONITORING_TIME):
            if self.control_tool.is_syslog_ng_running():
                socket_is_up = True
                break
            time.sleep(POLL_FREQ)
            atime = time.time()
        if not socket_is_up:
            logging.warning("The control socket is not created in a short time. socket: %s" % self.control_tool.get_syslog_ng_control_tool())
            return False
        return True


    def wait_for_stopping_control_socket(self):
        socket_is_up = False
        stime = atime = time.time()
        while atime <= (stime + MONITORING_TIME):
            if not self.control_tool.is_syslog_ng_running():
                socket_is_up = True
                break
            time.sleep(POLL_FREQ)
            atime = time.time()
        if not socket_is_up:
            logging.warning("The control socket is not created in a short time. socket: %s" % self.control_tool.get_syslog_ng_control_tool())
            return False
        return True


    def wait_for_statistics_count(self, stat):
        logging.info("stat: %s" % stat)
        found_stat = False
        stime = atime = time.time()
        while atime <= (stime + MONITORING_TIME):
            if stat in str(self.control_tool.stats()):
                found_stat = True
                break
            time.sleep(POLL_FREQ)
            atime = time.time()
        if not found_stat:
            logging.info(self.control_tool.stats())
            logging.warning("The following stat counter not reached: %s" % stat)
            return False
        return True
