#!/usr/bin/env python
#############################################################################
# Copyright (c) 2015-2018 Balabit
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

from src.syslog_ng_tester import *

@with_logpath(["file_source"], ["file_destination"])
@with_file_source("file_source", "input.log")
@with_file_destination("file_destination", "output.log")
class AcceptanceTester(SyslogNgTester):
    pass

def test_acceptance(tc):
    acceptance_tester = AcceptanceTester(tc)

    bsd_message = tc.create_dummy_bsd_message()
    bsd_log = tc.format_as_bsd(bsd_message)
    acceptance_tester.file_source.write_log(bsd_log, counter=3)

    acceptance_tester.start()

    output_logs = acceptance_tester.file_destination.read_logs(counter=3)
    expected_output_message = bsd_message.remove_priority()
    assert output_logs == tc.format_as_bsd_logs(expected_output_message, counter=3)
