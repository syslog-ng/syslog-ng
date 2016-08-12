from func_tests.testcase import Testcase
import time
import os
from shutil import copyfile


def get_config(input_file):
    return """@version: 3.8

source s_int { internal(); };
source s_file { file('%(input_file)s'); };

destination d_python {
    python(class(sngtestmod.DestTest)
           value-pairs(key('MSG') pair('HOST', 'bzorp') pair('DATE', '1970-04-01T13:37') key('MSGHDR')));
};

log { source(s_file); destination(d_python); };
""" % {'input_file': input_file}


def get_source_dir():
    return os.path.abspath(os.path.dirname(__file__))


UNIQID = 0


def registry_new_file(content):
    global UNIQID
    filename = "/tmp/slng-func-in.%d.tmp.txt" % UNIQID
    UNIQID += 1
    with open(filename, 'w') as file_object:
        file_object.write("%s\n" % content)
    return filename


def registry_new_dir():
    name = 'pytest.tmp'
    os.mkdir(name)
    return name


def test_destination():
    tc = Testcase()
    input_file = registry_new_file("input message 1\ninput message 2")
    output_dir = registry_new_dir()
    src_dir = get_source_dir()
    if 'PYTHONPATH' not in os.environ or src_dir not in os.environ['PYTHONPATH']:
        os.environ['PYTHONPATH'] = os.environ.get('PYTHONPATH', '') + ':' + src_dir

    tc.config.from_raw(get_config(input_file))

    os.chdir(output_dir)
    tc.syslog_ng.start()
    time.sleep(4)
    tc.syslog_ng.stop()

    result = tc.file_based_processor.get_messages_from_file('test-python.log')
    assert result == ['1970-04-01T13:37 bzorp input message 2 PYTHON!']