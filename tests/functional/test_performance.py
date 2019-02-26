#############################################################################
# Copyright (c) 2007-2015 Balabit
#
# This program is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License version 2 as published
# by the Free Software Foundation, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
#
# As an additional exemption you are allowed to compile & link against the
# OpenSSL libraries as published by the OpenSSL project. See the file
# COPYING for details.
#
#############################################################################

from globals import *
from log import *
from messagegen import *
from messagecheck import *

config = """@version: 3.20

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
