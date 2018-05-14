#############################################################################
# Copyright (c) 2007-2009 Balabit
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

import time

def print_user(msg):
    print '    ', time.strftime('%Y-%m-%dT%H:%M:%S'), msg

def print_start(testcase):
    print("\n\n##############################################")
    print("### Starting testcase: %s" % testcase)
    print("##############################################")
    print_user("Testcase start")

def print_end(testcase, result):
    print_user("Testcase end")
    print("##############################################")
    if result:
        print("### PASS: %s" % testcase)
    else:
        print("### FAIL: %s" % testcase)
    print("##############################################\n\n")

