#!/usr/bin/env python
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

import os, errno

#internal modules
from log import *
from messagegen import *
from control import *
from globals import *
import messagegen



def init_env():
    for pipe in ('log-pipe', 'log-padded-pipe'):
        try:
            os.unlink(pipe)
        except OSError:
            pass
        os.mkfifo(pipe)
    try:
        os.mkdir("wildcard")
    except OSError as e:
        if e.errno != errno.EEXIST:
            raise

def get_seed_input():
    raw_input =  """k/zFvqjGWdhStmhfOeTNtTs89P8soknF1J9kSQrz8hKdrjIutqTXMfIqCNUb7DXrMykMW+wKd1Pg
DwaUwxKmlaU1ItOek+jNUWVw9ZOSI1EmsXVgu+Hu7URgmeyY0A3WsDmMzR0Z2wTcRFSuINgBP8LC
8SG27gJZVOoVv09pfY9WyjvUYwg1jBdTfEM+qcDQKOACx4DH+SzO0bOOJMfMbR2iFaq18b/TCeQN
kRy9Lz2WvBsByQoXw/afxiu5xzn0MHoxTMCZCTjIyhGXzO/R2yj3eBVc5vxc5oxG3/EdjGnhmn/L
HEFVgc6EJ5OF1ye8hDIHFdZFehCAPbso3LL/3r8oJn+Axmc2sOrvhzMLpCV2KWdy8haiv3WZ9hZP
iJkC0pGBR+vhrdYE7qh2RzcdYoHRCvX5tpaOG+SE8+Az2WPHPOD7j4j+4KbhW+YbzYfUAsmwOQb4
R3vIyGO6Og4fg+AmKcycpv9Bo17zidbQe9zdZO6X4Df+ychK+yRSCMHLUyrEry7IFt60ivLAjc8b
Ladp70v7icWO+J9P/3pXIzKuQuaeT7J1q2xlCc/srV2pNV4+EhKJ8qSe+hG4LzpXWRGKo72h6BvL
Wgcp3S/wy1e6ov8XsVKjkWeQH8Nh3xOMWGYh6/yXSN44BLIBUdqk4I3TCKy1DawJQd1ivytEF0Xh
BstGzSoyeR92mN/K5/gy9wKFZffTbE7DmysKflRAN85ht7IVqxrTNXQ+UPWvlGZDAQ0NY45rQI3K
S1q0ahQivgGUZmEhuZkAUlYGWAqtCeDz3rZvrp5r2WJFGTZ+9yJZC5T2L+erDGBYxmcwxDzz8AvS
Ybp4aIBEtTK71cx9DtFTtSaQ6aW9rI5nc/owo0gv4Ddm5BYjMAd7kcc9TWnb1DZ9AJQAnSxgcuJM
acbYkFcqtMnafh/VnGekZ8yM0ZZqQPKBhysx+u2UBXib7Vvb6x6Y/xglNcqBWGFmzruKt6hx0pR2
x9IunzeIwdlcwIhLEKfIiy9ULwi7RTjVSeqgwucWoC0lw0BTotGeLDcFxTlQsE3T/UneLa8H6iSH
VW5iRZrvI0sdxt5Ud0TjNqXRGxuVczSuwpQwwxBn0ogr9DoRnp375PwGGh1/yqimW/+OStwP3cRR
yXEg6Zq1CvuYF/E6el4h9GylxkU7wEM2Ti9QJY4n3YsHyesalERqdd9xx5t7ADRodpMpZXoZGbrS
vccp3zMzS/aEZRuxky1/qjrAEh8OVA58e82jQqTdY8OQ/kKOu/gUgKBnHAvLkB/020p0CNbq6HjY
l625DLckaYmOPTh0ECFKzhaPF+/LNmzD36ToOAeuNjfbUjiUVGfntr2mc4E8mUFyo+TskrkSfw==
""".encode()

    import sys
    import base64
    if sys.version_info.major == 2:
      return base64.decodestring(raw_input)
    else:
      return base64.decodebytes(raw_input)


def seed_rnd():
    # Some platforms lack kernel supported random numbers, on those we have to explicitly seed the RNG.
    # using a fixed RND input does not matter as we are not protecting real data in this test program.
    try:
        import socket
        import ssl
        rnd = get_seed_input()
        ssl.RAND_add(rnd, 1024)
        if not ssl.RAND_status():
            raise "PRNG not seeded"
    except ImportError:
        return

def run_testcase(test_name, config, verbose, test_case):
    print_start(test_name)

    if not start_syslogng(config, verbose):
      sys.exit(1)

    print_user("Starting test case...")
    success = test_case()
    if not stop_syslogng():
      sys.exit(1)
    print_end(test_name, success)
    return success

# import test modules
import test_file_source
import test_filters
import test_input_drivers
import test_performance
import test_sql
import test_python
import test_map_value_pairs

tests = (test_input_drivers, test_sql, test_file_source, test_filters, test_performance, test_python, test_map_value_pairs)
failed_tests = []

init_env()
seed_rnd()


verbose = False
if len(sys.argv) > 1:
    verbose = True
try:
    for test_module in tests:
        if hasattr(test_module, "check_env") and not test_module.check_env():
            continue


        contents = dir(test_module)
        contents.sort()
        for obj in contents:
            if obj[:5] != 'test_':
                continue
            test_case = getattr(test_module, obj)

            if type(test_module.config) is str:
                test_case_name = test_module.__name__ + '.' + obj
                if not run_testcase(test_case_name, test_module.config, verbose, test_case):
                    failed_tests.append(test_case_name)

            elif type(test_module.config) is dict:
                for config_name in test_module.config:
                    testcase_name = "%s.%s[%s]" %(test_module.__name__, obj, config_name)
                    config = test_module.config[config_name]
                    if not run_testcase(testcase_name, config, verbose, test_case):
                        failed_tests.append(testcase_name)

finally:
    stop_syslogng()

if len(failed_tests)>0:
    print("List of failed test cases: %s" % ','.join(failed_tests))
    sys.exit(1)

sys.exit(0)
