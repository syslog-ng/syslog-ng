from globals import *
from log import *
from messagegen import *
from messagecheck import *

config = """@version: 3.6

options { ts_format(iso); chain_hostnames(no); keep_hostname(yes); threaded(yes); };

source s_int { internal(); };
source s_tcp { tcp(port(%(port_number)d)); };

destination d_messages { file("test-performance.log"); };

log { source(s_tcp); destination(d_messages); };

""" % locals()

def test_performance():
    expected_rate = {
      'bzorp': 10000
    }
    print_user("Starting loggen for 10 seconds")
    out = os.popen("../loggen/loggen -r 1000000 -Q -i -S -s 160 -I 10 127.0.0.1 %d 2>&1 |tail -n +1" % port_number, 'r').read()

    print_user("performane: %s" % out)
    rate = float(re.sub('^.*rate = ([0-9.]+).*$', '\\1', out))

    hostname = os.uname()[1]
    if expected_rate.has_key(hostname):
        return rate > expected_rate[hostname]

    # we expect to be able to process at least 1000 msgs/sec even on our venerable HP-UX
    return rate > 100
