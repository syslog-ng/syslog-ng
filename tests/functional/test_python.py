#############################################################################
# Copyright (c) 2015 Balabit
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

config = """@version: 3.25

options { keep-hostname(yes); };

source s_int { internal(); };
source s_tcp { tcp(port(%(port_number)d)); };

destination d_python {
    python(class(sngtestmod.DestTest)
           value-pairs(key('MSG') pair('HOST', 'bzorp') pair('DATE', '$ISODATE') key('MSGHDR')));
};

log { source(s_tcp); destination(d_python); };

python {

class MyParser(object):
  def init(self, options):
      return True

  def deinit(self):
      return True

  def parse(self, msg):
      msg['FOOBAR'] = msg['MSG']
      return True

};

log {
  source(s_tcp);
  parser {
      python(class("MyParser"));
  };
  destination { file("test-python-parser.log" template("$ISODATE $HOST $MSGHDR$FOOBAR\n")); };
};

""" % locals()


def get_source_dir():
    return os.path.abspath(os.path.dirname(__file__))

def check_env():

    if not has_module('mod-python'):
        print 'Python module is not available, skipping Python test'
        return False

    src_dir = get_source_dir()
    print src_dir
    if 'PYTHONPATH' not in os.environ or src_dir not in os.environ['PYTHONPATH']:
        os.environ['PYTHONPATH'] = os.environ.get('PYTHONPATH', '') + ':' + src_dir

    print 'Python module found, proceeding to Python tests'
    return True


def test_python():

    messages = (
        'python1',
        'python2'
    )
    s = SocketSender(AF_INET, ('localhost', port_number), dgram=0)

    expected = []
    for msg in messages:
        expected.extend(s.sendMessages(msg, pri=7))
    flush_files(2)
    stopped = stop_syslogng()
    if not stopped or not check_file_expected('test-python', expected, settle_time=2):
        return False
    return True

def test_python_parser():

    messages = (
        'python_parser1',
        'python_parser2'
    )
    s = SocketSender(AF_INET, ('localhost', port_number), dgram=0)

    expected = []
    for msg in messages:
        expected.extend(s.sendMessages(msg, pri=7))
    flush_files(2)
    stopped = stop_syslogng()
    if not stopped or not check_file_expected('test-python-parser', expected, settle_time=2):
        return False
    return True
