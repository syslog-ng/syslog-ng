import logging
import psutil


def is_concurrent_syslog_daemon_running():
    for proc in psutil.process_iter():
        if "syslog" in proc.as_dict(attrs=['name'])['name']:
            logging.error("Concurrent syslog daemon running with name [%s], pid [%s]. Please stop it before continue." % (proc.as_dict(attrs=['name'])['name'], proc.as_dict(attrs=['pid'])['pid']))
            raise SystemExit(1)
