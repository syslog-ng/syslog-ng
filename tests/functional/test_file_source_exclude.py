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
from control import *

config = """@version: %(syslog_ng_version)s

options { ts_format(iso); chain_hostnames(no); keep_hostname(yes); threaded(yes); };

source s_wildcard { wildcard_file(
                    base_dir("wildcard")
                    filename_pattern("*.log")
                    exclude_pattern("*.?.log")
                    monitor-method("auto")
                    recursive(yes));
                  };

destination d_wildcard { file("test-wildcard.log"); };

log { source(s_wildcard); destination(d_wildcard); };

""" % locals()

settle_time=4

def create_source_files(recursive=False):
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
    expected = []

    for ndx in range(0, len(messages)):
        if not recursive:
            s = FileSender('wildcard/%d.log' % (ndx % 4), repeat=100)
        else:
            s = FileSender('wildcard/wildcard%d/%d.log' % ((ndx % 2),(ndx % 4)), repeat=100)
        expected.extend(s.sendMessages(messages[ndx]))
    return expected

def create_excluded_files(recursive=False):
    messages = (
      'excluded0',
      'excluded1',
      'excluded2',
      'excluded3',
      'excluded4',
      'excluded5',
      'excluded6',
      'excluded7',
    )
    expected = []

    for ndx in range(0, len(messages)):
        if not recursive:
            s = FileSender('wildcard/excluded.%d.log' % (ndx % 4), repeat=100)
        else:
            s = FileSender('wildcard/wildcard%d/excluded.%d.log' % ((ndx % 2),(ndx % 4)), repeat=100)
        expected.extend(s.sendMessages(messages[ndx]))
    return expected

def test_excluded_files():
    expected = create_source_files()
    create_excluded_files()  # not expected

    if not check_file_expected('test-wildcard', expected, settle_time=settle_time):
        return False
    return True
