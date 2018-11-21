#!/usr/bin/env python
#############################################################################
# Copyright (c) 2019 Balabit
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

from pathlib2 import Path
import pytest

class FileReopenTest(object):
    def __init__(self, testcase):
        self.testcase = testcase
        self.config = testcase.new_config()
        self.file_source = self.config.create_file_source(file_name="input.log")
        source_group = self.config.create_source_group(self.file_source)

        self.file_destination = self.config.create_file_destination(file_name="output.log")
        destination_group = self.config.create_destination_group(self.file_destination)

        self.new_file_destination = self.config.create_file_destination(file_name="output.old")

        self.config.create_logpath(sources=source_group, destinations=destination_group)

        self.syslog_ng = testcase.new_syslog_ng()

        self.syslog_ng_ctl = testcase.new_syslog_ng_ctl()

    def start(self):
        self.syslog_ng.start(self.config)

    def send_message(self):
        self.bsd_log = self.testcase.format_as_bsd(self.testcase.create_dummy_bsd_message())
        self.file_source.write_log(self.bsd_log)

    def rename_destination(self):
        orig_filename = self.file_destination.get_path()
        orig_filename.rename(Path(orig_filename.parent, orig_filename.stem + ".old"))

    def read_logs_from_new_file(self):
        return self.new_file_destination.read_logs(counter=2)

    def read_logs_from_original_file(self):
        return self.file_destination.read_logs(counter=1)

    def reopen(self):
        self.syslog_ng_ctl.reopen()

@pytest.mark.slow
def test_file_reopen(tc):

    reopen_tester = FileReopenTest(tc)
    path = reopen_tester.file_destination.get_path()

    reopen_tester.send_message()

    reopen_tester.start()
    assert path.exists()

    reopen_tester.rename_destination()

    reopen_tester.send_message()
    reopen_tester.read_logs_from_new_file();
    assert not path.exists()

    reopen_tester.reopen()

    reopen_tester.send_message()
    reopen_tester.read_logs_from_original_file();
    assert path.exists()
