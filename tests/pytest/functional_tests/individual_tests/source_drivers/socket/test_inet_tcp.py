from functional_tests.common.testcase import TC
import os


def get_config(input_port):
    return """@version: 3.8

source s_inet { tcp(port(%(port_number)d)); so_rcvbuf(131072)); };

destination d_file {
    file('%(out_file)');
};

log { source(s_inet); destination(d_file); };
""" % {'input_file': input_port}


def get_source_dir():
    return os.path.abspath(os.path.dirname(__file__))


tc = TC()


def registry_new_file(content):
    global tc
    filename = tc.global_register.get_uniq_filename()
    with open(filename, 'w') as file_object:
        file_object.write("%s\n" % content)
    return filename


def registry_new_dir():
    global tc
    name = tc.global_register.get_uniq_dirname()
    os.mkdir(name)
    return name


def consume_message_from_file(file):
    messages = tc.file_based_processor.get_messages_from_file(file)
    os.remove(file)
    return messages


def test_destination():
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

    result = consume_message_from_file('test-python.log')
    assert result == ['1970-04-01T13:37 bzorp input message 2 PYTHON!']
