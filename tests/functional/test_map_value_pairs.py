#############################################################################
# Copyright (c) 2017 Balabit
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
from control import flush_files, stop_syslogng
import os

config = """@version: 3.16

options { keep-hostname(yes); };

source s_int { internal(); };
source s_tcp { tcp(port(%(port_number)d)); };

parser p_map {
  map-value-pairs(key("MSG*" rekey(add-prefix("foo."))));
};

log {
  source(s_tcp);
  parser(p_map);
  destination { file("test-map-value-pairs.log" template("$ISODATE $HOST $MSGHDR${foo.MSG}\n")); };
};

""" % locals()


def get_source_dir():
    return os.path.abspath(os.path.dirname(__file__))

def test_map_value_pairs():

    messages = (
        'map_value_pairs1',
        'map_value_pairs2'
    )
    s = SocketSender(AF_INET, ('localhost', port_number), dgram=0)

    expected = []
    for msg in messages:
        expected.extend(s.sendMessages(msg, pri=7))
    stopped = stop_syslogng()
    if not stopped or not check_file_expected('test-map-value-pairs', expected, settle_time=2):
        return False
    return True
