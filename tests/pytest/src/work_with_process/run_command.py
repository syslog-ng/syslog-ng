import subprocess
import logging

def run_command_for_pid(command):
    logging.info("command for pid: --> [%s]" % command)
    try:
        return subprocess.Popen(command, stderr=subprocess.PIPE, stdout=subprocess.PIPE, shell=True)
    except OSError as error:
        logging.error("Error during subprocess.popen: %s" % error)
        ## FIXME: dump testrun log
        raise SystemExit(1)

def run_command_for_output(command):
    logging.info("command for output: --> [%s]" % command)
    try:
        return subprocess.check_output(command, stderr=subprocess.PIPE, shell=True)
    except OSError as error:
        logging.error("Error during subprocess.check_output: %s" % error)
        ## FIXME: dump testrun log
        raise SystemExit(1)

def run_command_for_exit_code(command):
    logging.info("command for exit_code: --> [%s]" % command)
    try:
        return subprocess.call(command, stderr=subprocess.PIPE, stdout=subprocess.PIPE, shell=True)
    except OSError as error:
        logging.error("Error during subprocess.call: %s" % error)
        ## FIXME: dump testrun log
        raise SystemExit(1)