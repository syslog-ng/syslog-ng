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

config = """@version: 3.13

options { ts_format(iso); chain_hostnames(no); keep_hostname(yes); threaded(yes); };

source s_int { internal(); };
source s_unix { unix-stream("log-stream" flags(expect-hostname)); };

filter f_facility { message("facility"); };
filter f_facility1 { facility(syslog); };
filter f_facility2 { facility(kern); };
filter f_facility3 { facility(mail); };
filter f_facility4 { facility(daemon,auth,lpr); };

destination d_facility1 { file("test-facility1.log"); logstore("test-facility1.lgs"); };
destination d_facility2 { file("test-facility2.log"); logstore("test-facility2.lgs"); };
destination d_facility3 { file("test-facility3.log"); logstore("test-facility3.lgs"); };
destination d_facility4 { file("test-facility4.log"); logstore("test-facility4.lgs"); };

log { source(s_unix);
    filter(f_facility);
    log { filter(f_facility1); destination(d_facility1); };
    log { filter(f_facility2); destination(d_facility2); };
    log { filter(f_facility3); destination(d_facility3); };
    log { filter(f_facility4); destination(d_facility4); };
};

filter f_level { message("level"); };
filter f_level1 { level(debug); };
filter f_level2 { level(info); };
filter f_level3 { level(notice); };
filter f_level4 { level(warning..crit); };

destination d_level1 { file("test-level1.log"); logstore("test-level1.lgs"); };
destination d_level2 { file("test-level2.log"); logstore("test-level2.lgs"); };
destination d_level3 { file("test-level3.log"); logstore("test-level3.lgs"); };
destination d_level4 { file("test-level4.log"); logstore("test-level4.lgs"); };

log { source(s_unix);
    filter(f_level);
    log { filter(f_level1); destination(d_level1); };
    log { filter(f_level2); destination(d_level2); };
    log { filter(f_level3); destination(d_level3); };
    log { filter(f_level4); destination(d_level4); };
};

""" % locals()

def test_facility_single():
    messages = (
      (41, 'facility1'),
      (1, 'facility2'),
      (17, 'facility3'),
    )
    expected = [None,] * len(messages)

    s = SocketSender(AF_UNIX, 'log-stream', dgram=0, repeat=10)
    for ndx in range(0, len(messages)):
        if not expected[ndx]:
            expected[ndx] = []
        expected[ndx].extend(s.sendMessages(messages[ndx][1], pri=messages[ndx][0]))

    for ndx in range(0, len(messages)):
        if not check_file_expected('test-facility%d' % (ndx + 1,), expected[ndx]):
            return False
    return True

def test_facility_multi():
    messages = (
      (25, 'facility4'),
      (33, 'facility4'),
      (49, 'facility4'),
    )
    expected = []

    s = SocketSender(AF_UNIX, 'log-stream', dgram=0, repeat=10)
    for ndx in range(0, len(messages)):
        expected.extend(s.sendMessages(messages[ndx][1], pri=messages[ndx][0]))

    if not check_file_expected('test-facility4', expected):
        return False
    return True


def test_level_single():
    messages = (
      (7, 'level1'),
      (6, 'level2'),
      (5, 'level3'),
    )
    expected = [None,] * len(messages)

    s = SocketSender(AF_UNIX, 'log-stream', dgram=0, repeat=10)
    for ndx in range(0, len(messages)):
        if not expected[ndx]:
            expected[ndx] = []
        expected[ndx].extend(s.sendMessages(messages[ndx][1], pri=messages[ndx][0]))

    for ndx in range(0, len(messages)):
        if not check_file_expected('test-level%d' % (ndx + 1,), expected[ndx]):
            return False
    return True

def test_level_multi():
    messages = (
      (4, 'level4'),
      (3, 'level4'),
      (2, 'level4'),
    )
    expected = []

    s = SocketSender(AF_UNIX, 'log-stream', dgram=0, repeat=10)
    for ndx in range(0, len(messages)):
        expected.extend(s.sendMessages(messages[ndx][1], pri=messages[ndx][0]))

    if not check_file_expected('test-level4', expected):
        return False
    return True
