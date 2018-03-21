import socket
from source.testdb.initializer.setup_classes import SetupClasses


def init_unittest(request):
    sc = SetupClasses(request)
    return sc


def test_create_message_for_source_driver(request):
    sc = init_unittest(request)
    expected_messages = [
        '<38>Jun  1 08:05:04 %s testprogram[9999]: test ÁÉŐÚŰÓÜ-ááéúóö message 1' % socket.gethostname(),
        '<38>Jun  1 08:05:04 %s testprogram[9999]: test ÁÉŐÚŰÓÜ-ááéúóö message 2' % socket.gethostname()
    ]
    assert sc.message_interface.create_message_for_source_driver(driver_name="file", counter=2) == expected_messages


def test_create_message_for_destination_driver(request):
    sc = init_unittest(request)
    expected_messages = [
        'Jun  1 08:05:04 %s testprogram[9999]: test ÁÉŐÚŰÓÜ-ááéúóö message 1\n' % socket.gethostname(),
    ]
    assert sc.message_interface.create_message_for_destination_driver(driver_name="file") == expected_messages


def test_create_bsd_message(request):
    sc = init_unittest(request)
    expected_messages = "<38>Jun  1 08:05:04 %s testprogram[9999]: test ÁÉŐÚŰÓÜ-ááéúóö message 1" % socket.gethostname()
    assert sc.message_interface.create_bsd_message() == expected_messages


def test_create_multiple_bsd_messages(request):
    sc = init_unittest(request)
    expected_messages = [
        '<38>Jun  1 08:05:04 %s testprogram[9999]: test ÁÉŐÚŰÓÜ-ááéúóö message 1' % socket.gethostname(),
        '<38>Jun  1 08:05:04 %s testprogram[9999]: test ÁÉŐÚŰÓÜ-ááéúóö message 2' % socket.gethostname()
    ]
    assert sc.message_interface.create_multiple_bsd_messages() == expected_messages


def test_create_bsd_message_with_defined_message_parts(request):
    sc = init_unittest(request)
    expected_messages = "<99>Dec  1 08:05:04 test-machine test-program[1111]: Test message 1"
    defined_bsd_message_parts = {
        "priority": "99",
        "bsd_timestamp": "Dec  1 08:05:04",
        "hostname": "test-machine",
        "program": "test-program",
        "pid": "1111",
        "message": "Test message"
    }
    assert sc.message_interface.create_bsd_message(
        defined_bsd_message_parts=defined_bsd_message_parts) == expected_messages


def test_create_ietf_message(request):
    sc = init_unittest(request)
    expected_messages = '124 <38>1 2017-06-01T08:05:04+02:00 %s testprogram 9999 - [meta sequenceId="1"] test ÁÉŐÚŰÓÜ-ááéúóö message 1' % socket.gethostname()
    assert sc.message_interface.create_ietf_message() == expected_messages


def test_create_multiple_ietf_message(request):
    sc = init_unittest(request)
    expected_messages = [
        '124 <38>1 2017-06-01T08:05:04+02:00 %s testprogram 9999 - [meta sequenceId="1"] test ÁÉŐÚŰÓÜ-ááéúóö message 1' % socket.gethostname(),
        '124 <38>1 2017-06-01T08:05:04+02:00 %s testprogram 9999 - [meta sequenceId="2"] test ÁÉŐÚŰÓÜ-ááéúóö message 2' % socket.gethostname()
    ]
    assert sc.message_interface.create_multiple_ietf_messages() == expected_messages


def test_create_ietf_message_with_defined_message_parts(request):
    sc = init_unittest(request)
    expected_messages = '102 <99>99 2099-09-09T08:05:04+02:00 test-machine test-program 1111 9 [meta sequenceId="1"] Test message 1'
    defined_ietf_message_parts = {
        "priority": "99",
        "syslog_protocol_version": "99",
        "iso_timestamp": "2099-09-09T08:05:04+02:00",
        "hostname": "test-machine",
        "program": "test-program",
        "pid": "1111",
        "message_id": "9",
        "sdata": '[meta sequenceId="99"]',
        "message": "Test message"
    }
    assert sc.message_interface.create_ietf_message(
        defined_ietf_message_parts=defined_ietf_message_parts) == expected_messages
