from globals import *
from log import *
from messagegen import *
from messagecheck import *
from control import flush_files, stop_syslogng
import os

config = """@version: 3.7

source s_int { internal(); };
source s_tcp { tcp(port(%(port_number)d)); };

destination d_python {
    python(class(sngtestmod.DestTest)
           value-pairs(key('MSG') pair('HOST', 'bzorp') pair('DATE', '$ISODATE') key('MSGHDR')));
};

log { source(s_tcp); destination(d_python); };

""" % locals()


def get_source_dir():
    return os.path.abspath(os.path.dirname(__file__))

def check_env():

    if not has_module('mod-python'):
        print 'Python module is not available, skipping Python test'
        return False

    src_dir = get_source_dir()
    print src_dir
    if 'PYTHONPATH' not in os.environ or src_dir not in os.environ['PYTHONPATH']:
        os.environ['PYTHONPATH'] = os.environ.get('PYTHONPATH', '') + ':' + src_dir

    print 'Python module found, proceeding to Python tests'
    return True


def test_python():

    messages = (
        'python1',
        'python2'
    )
    s = SocketSender(AF_INET, ('localhost', port_number), dgram=0)

    expected = []
    for msg in messages:
        expected.extend(s.sendMessages(msg, pri=7))
    stopped = stop_syslogng()
    if not stopped or not check_file_expected('test-python', expected, settle_time=2):
        return False
    return True
