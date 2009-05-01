from globals import *
from log import *
from messagegen import *
from messagecheck import *

config = """@version: 3.0

options { use_dns(no); };

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
    out = os.popen("../loggen/loggen --rate 1000000 --inet --stream --size 120 --interval 10 127.0.0.1 %d 2>&1 |tail -1" % port_number, 'r').read()

    print_user("performane: %s" % out)
    rate = float(re.sub('^.*rate = ([0-9.]+).*$', '\\1', out))

    hostname = os.uname()[1]
    if expected_rate.has_key(hostname):
        return rate > expected_rate[hostname]

    # we expect to be able to process at least 1000 msgs/sec even on our venerable HP-UX
    return rate > 1000
