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

config = """@version: 3.8

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
