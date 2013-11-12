from globals import *
from log import *
from messagegen import *
from messagecheck import *

config = """@version: 3.6

options { ts_format(iso); chain_hostnames(no); keep_hostname(yes); threaded(yes); };

source s_int { internal(); };
source s_wildcard { file("wildcard/*.log"); };

destination d_wildcard { file("test-wildcard.log"); logstore("test-wildcard.lgs"); };

log { source(s_wildcard); destination(d_wildcard); };

""" % locals()

def test_wildcard_files():
    messages = (
      'wildcard0',
      'wildcard1',
      'wildcard2',
      'wildcard3',
      'wildcard4',
      'wildcard5',
      'wildcard6',
      'wildcard7',
    )

    if not wildcard_file_source_supported:
        print_user("Not testing a Premium version, skipping wild card source tests")
        return True
    expected = []

    for ndx in range(0, len(messages)):
        s = FileSender('wildcard/%d.log' % (ndx % 4), repeat=100)
        expected.extend(s.sendMessages(messages[ndx]))

    # we need more time to settle, as poll based polling might need 4*3 sec
    # to read the contents for all 4 files (1 sec to discover there are
    # messages)

    if not check_file_expected('test-wildcard', expected, settle_time=12):
        return False
    return True
