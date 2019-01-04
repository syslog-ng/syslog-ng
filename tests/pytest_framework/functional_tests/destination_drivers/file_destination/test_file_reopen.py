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

def with_file_source(variable_name, file_name):
    def decorator(cls):
        def add_file_source(self, variable_name, file_name):
            setattr(self, variable_name, self.config.create_file_source(file_name=file_name))
        cls.add_init_task(cls, add_file_source, (variable_name, file_name))
        return cls
    return decorator

def with_file_destination(variable_name, file_name):
    def decorator(cls):
        def add_file_destination(self, variable_name, file_name):
            setattr(self, variable_name, self.config.create_file_destination(file_name=file_name))
        cls.add_init_task(cls, add_file_destination, (variable_name, file_name))
        return cls
    return decorator

class SyslogNgTester(object):
    def __init__(self, testcase):
        self.testcase = testcase
        self.config = testcase.new_config()
        self.syslog_ng = testcase.new_syslog_ng()
        self.syslog_ng_ctl = testcase.new_syslog_ng_ctl()

        for func, args in self.init_functions:
            func(self, *args)

    def start(self):
        self.syslog_ng.start(self.config)

    def reopen(self):
        self.syslog_ng_ctl.reopen()

    @staticmethod
    def add_init_task(cls, func, args):
        assert cls != SyslogNgTester

        if not hasattr(cls, "init_functions"):
            cls.init_functions = []
        cls.init_functions.append((func, args))

@with_file_source("file_source", "input.log")
@with_file_destination("file_destination", "output.log")
@with_file_destination("new_file_destination", "output.old")
class FileReopenTest(SyslogNgTester):
    def __init__(self, testcase):
        SyslogNgTester.__init__(self, testcase)
        source_group = self.config.create_source_group(self.file_source)
        destination_group = self.config.create_destination_group(self.file_destination)
        self.config.create_logpath(sources=source_group, destinations=destination_group)

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
